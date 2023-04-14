
#ifndef __BACKHAUL_H__
#define __BACKHAUL_H__

#define TOKEN_BYTES 88
#define MAX_TOKENS  1000

#define APPSERVER_URL "https://appserver.example.com"
#define PROVIDER_URL "https://provider.example.com"

void backhaul_reset();
int backhaul_upload_data(char *payload, int payload_len);
int backhaul_num_tokens();
int backhaul_redeem_tokens();

#endif // __BACKHAUL_H__