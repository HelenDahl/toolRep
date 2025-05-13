/*
 * Copyright (c) 2025 Institute of Parallel And Distributed Systems (IPADS),
 * Shanghai Jiao Tong University (SJTU) Licensed under the Mulan PSL v2. You can
 * use this software according to the terms and conditions of the Mulan PSL v2.
 * You may obtain a copy of Mulan PSL v2 at:
 *     http://license.coscl.org.cn/MulanPSL2
 * THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY
 * KIND, EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO
 * NON-INFRINGEMENT, MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. See the
 * Mulan PSL v2 for more details.
 */

#include <arpa/inet.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "config.h"
#include "json/simple_json.h"

__attribute__((unused)) static const char *json_object_name = "llama_client";

void handle_sigint(int signum) {
    (void)signum;
    printf("\nClient interrupted by user.\n");
    exit(0);
}

int connect_to_server() {
    int sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("socket");
        return -1;
    }

    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_ADDR, &serv_addr.sin_addr) <= 0) {
        perror("inet_pton");
        close(sockfd);
        return -1;
    }

    if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("connect");
        close(sockfd);
        return -1;
    }

    printf("Connected to %s:%d\n", SERVER_ADDR, SERVER_PORT);
    return sockfd;
}

char *get_input() {
    char *input = NULL;
    size_t len = 0;
    ssize_t nread = 0;
    do {
        printf("> ");
        fflush(stdout);
        nread = getline(&input, &len, stdin);
    } while (nread <= 1);

    input[nread - 1] = '\0';
    return input;
}

int main() {
    signal(SIGINT, handle_sigint);

    int sockfd = connect_to_server();
    if (sockfd < 0) return -1;

    json_object *root = json_object_new_object();

    while (1) {
        char *input = get_input();
        if (!input) continue;

        json_object_object_add(root, "token", json_object_new_string(input));
        json_object_object_add(root, "eog", json_object_new_boolean(1));

        const char *json_str = json_object_to_json_string(root);
        send(sockfd, json_str, strlen(json_str), 0);

        char buffer[MAX_BUFFER_SIZE];
        ssize_t n = recv(sockfd, buffer, sizeof(buffer)-1, 0);
        if (n <= 0) break;
        buffer[n] = '\0';

        json_object *res = json_tokener_parse(buffer);
        json_object *token;
        json_object_object_get_ex(res, "token", &token);
        printf("%s\n", json_object_get_string(token));

        free(input);
        json_object_put(res);
    }

    close(sockfd);
    json_object_put(root);
    return 0;
}