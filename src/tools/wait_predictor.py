#!/usr/bin/env python3
import sys
import os
import io
import warnings
import argparse
import pandas as pd
import joblib
from sklearn.ensemble import RandomForestRegressor
from sklearn.model_selection import train_test_split
from sklearn.metrics import mean_absolute_error
from datetime import datetime

# Suppress sklearn warnings
warnings.filterwarnings('ignore', category=UserWarning)

# Fix Unicode output on Windows
if sys.platform == "win32":
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf-8')
    sys.stderr = io.TextIOWrapper(sys.stderr.buffer, encoding='utf-8')

MODEL_PATH = os.path.join(os.path.dirname(__file__), "wait_model.joblib")

def find_served_csv():
    candidates = [
        os.path.join(os.getcwd(), "data", "served.csv"),
        os.path.join(os.path.dirname(__file__), "..", "..", "data", "served.csv"),
        os.path.join(os.path.dirname(__file__), "..", "data", "served.csv"),
        os.path.join(os.path.dirname(__file__), "data", "served.csv"),
    ]
    for p in candidates:
        p = os.path.normpath(p)
        if os.path.isfile(p):
            return p
    raise FileNotFoundError("data/served.csv not found.")

def load_data(path=None):
    path = path or find_served_csv()
    df = pd.read_csv(path, comment='#', skip_blank_lines=True)
    df = df.rename(columns=lambda s: s.strip())
    if 'Arrival' not in df.columns or 'Severity' not in df.columns:
        raise SystemExit("served.csv missing required columns.")
    wait_col = None
    for cand in ['Wait(sec)', 'Wait_sec', 'wait_sec']:
        if cand in df.columns:
            wait_col = cand
            break
    if not wait_col:
        wait_col = df.columns[7] if df.shape[1] >= 8 else df.columns[-2]
    df['Arrival_parsed'] = pd.to_datetime(df['Arrival'], errors='coerce')
    df['hour'] = df['Arrival_parsed'].dt.hour.fillna(0).astype(int)
    df['age'] = pd.to_numeric(df['Age'], errors='coerce').fillna(df['Age'].median())
    df['severity'] = pd.to_numeric(df['Severity'], errors='coerce').fillna(0).astype(int)
    df['wait_sec'] = pd.to_numeric(df[wait_col], errors='coerce').fillna(0)
    df = df[df['wait_sec'] > 0]
    if df.shape[0] < 5:
        raise SystemExit("Not enough data to train.")
    return df

def train(args):
    df = load_data()
    X = df[['severity', 'age', 'hour']]
    y = df['wait_sec']
    X_train, X_test, y_train, y_test = train_test_split(X, y, test_size=0.2, random_state=42)
    model = RandomForestRegressor(n_estimators=50, random_state=42)
    model.fit(X_train, y_train)
    preds = model.predict(X_test)
    mae = mean_absolute_error(y_test, preds)
    joblib.dump(model, MODEL_PATH)
    print(f"Trained model saved to {MODEL_PATH}  MAE={mae:.1f} sec ({mae/60:.2f} min)")

def predict(args):
    if not os.path.isfile(MODEL_PATH):
        print("Model not found. Train first: python src/tools/wait_predictor.py train")
        sys.exit(1)
    
    model = joblib.load(MODEL_PATH)
    sev = int(args.severity)
    age = float(args.age)
    hour = 0
    
    if args.arrival:
        for fmt in ("%Y-%m-%d %H:%M:%S", "%Y-%m-%dT%H:%M:%S", "%Y-%m-%d"):
            try:
                hour = datetime.strptime(args.arrival, fmt).hour
                break
            except:
                continue
    
    X = pd.DataFrame([[sev, age, hour]], columns=['severity', 'age', 'hour'])
    pred_sec = float(model.predict(X)[0])
    pred_min = max(0, round(pred_sec / 60.0))
    
    print(f"Predicted wait ~{pred_min} minutes ({int(pred_sec)} sec)")
    print(f"PREDICT_SEC:{int(pred_sec)}")

def cli():
    parser = argparse.ArgumentParser(description="ML-based wait time predictor")
    sub = parser.add_subparsers(dest='cmd')
    sub.add_parser('train')
    p = sub.add_parser('predict')
    p.add_argument('--severity', required=True, help='0=Normal,1=Serious,2=Critical')
    p.add_argument('--age', required=True, help='patient age')
    p.add_argument('--arrival', required=False, help='arrival datetime')
    args = parser.parse_args()
    if args.cmd == 'train':
        train(args)
    elif args.cmd == 'predict':
        predict(args)
    else:
        parser.print_help()

if __name__ == "__main__":
    cli()