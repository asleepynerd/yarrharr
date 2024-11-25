#pragma once
#include <curl/curl.h>
#include <cstdio>
#include <sys/ioctl.h>
#include <unistd.h>
#include <chrono>
#include <iostream>
#include <sstream>
#include <iomanip>

size_t writeCallback(void* ptr, size_t size, size_t nmemb, FILE* stream);
int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow); 