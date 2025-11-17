#ifndef AUTH_H
#define AUTH_H

#include <stdbool.h>
#include <stddef.h>

bool auth_create_user(const char* users_file);
bool auth_login(const char* users_file);
bool auth_login_attempts(const char* users_file, int attempts);
void read_password(char *password, size_t max_len);

#endif // AUTH_H