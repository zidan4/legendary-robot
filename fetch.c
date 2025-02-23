
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curl/curl.h>
#include <sqlite3.h>

// Callback to write fetched data into a string
size_t WriteCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    char** response_ptr = (char**)userp;

    *response_ptr = realloc(*response_ptr, strlen(*response_ptr) + totalSize + 1);
    if (*response_ptr == NULL) {
        fprintf(stderr, "Memory allocation failed\n");
        return 0;
    }

    strncat(*response_ptr, contents, totalSize);
    return totalSize;
}

// Function to initialize SQLite and create table
int init_database(sqlite3** db) {
    int rc = sqlite3_open("api_data.db", db);
    if (rc) {
        fprintf(stderr, "Can't open database: %s\n", sqlite3_errmsg(*db));
        return rc;
    }

    const char* create_table_sql = "CREATE TABLE IF NOT EXISTS api_response ("
                                   "id INTEGER PRIMARY KEY AUTOINCREMENT,"
                                   "data TEXT NOT NULL);";

    char* errMsg = 0;
    rc = sqlite3_exec(*db, create_table_sql, 0, 0, &errMsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "SQL Error: %s\n", errMsg);
        sqlite3_free(errMsg);
    }

    return rc;
}

// Function to insert API data into SQLite
int insert_data(sqlite3* db, const char* data) {
    const char* insert_sql = "INSERT INTO api_response (data) VALUES (?);";
    sqlite3_stmt* stmt;

    int rc = sqlite3_prepare_v2(db, insert_sql, -1, &stmt, 0);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Failed to prepare statement\n");
        return rc;
    }

    sqlite3_bind_text(stmt, 1, data, -1, SQLITE_STATIC);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Failed to insert data\n");
    } else {
        printf("Data inserted into database successfully.\n");
    }

    sqlite3_finalize(stmt);
    return rc;
}

int main() {
    CURL* curl;
    CURLcode res;
    char* response = calloc(1, sizeof(char));
    sqlite3* db;

    if (!response) {
        fprintf(stderr, "Memory allocation failed\n");
        return 1;
    }

    // Initialize SQLite
    if (init_database(&db) != SQLITE_OK) {
        free(response);
        return 1;
    }

    // Initialize cURL
    curl_global_init(CURL_GLOBAL_DEFAULT);
    curl = curl_easy_init();

    if (curl) {
        const char* url = "https://jsonplaceholder.typicode.com/posts/1";

        curl_easy_setopt(curl, CURLOPT_URL, url);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

        res = curl_easy_perform(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "cURL Error: %s\n", curl_easy_strerror(res));
        } else {
            printf("API Response:\n%s\n", response);
            // Insert API response into SQLite database
            insert_data(db, response);
        }

        curl_easy_cleanup(curl);
    } else {
        fprintf(stderr, "Failed to initialize cURL\n");
    }

    curl_global_cleanup();
    sqlite3_close(db);
    free(response);
    return 0;
}
