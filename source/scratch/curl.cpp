#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL2_gfxPrimitives.h>
#include <SDL2/SDL_image.h>
#include <curl/curl.h>
#include <coreinit/memory.h>
#include <map>
#include <string>
#include <filesystem>
#include <iostream>
#include <vector>

#include <curl/curl.h>

struct MemoryBuffer {
    std::vector<unsigned char> data;
};

static size_t WriteMemoryCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t totalSize = size * nmemb;
    MemoryBuffer* mem = (MemoryBuffer*)userp;
    unsigned char* ptr = (unsigned char*)contents;
    mem->data.insert(mem->data.end(), ptr, ptr + totalSize);
    return totalSize;
}

std::string FetchJson(const char* url) {
    CURL* curl = curl_easy_init();
    if (!curl) return nullptr;

    MemoryBuffer buffer;

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || buffer.data.empty()) return "";

    std::string result(buffer.data.begin(), buffer.data.end());
    return result;
}