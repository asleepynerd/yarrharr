#include "download_utils.hpp"

size_t writeCallback(void* ptr, size_t size, size_t nmemb, FILE* stream) {
    return fwrite(ptr, size, nmemb, stream);
}

int progressCallback(void* clientp, curl_off_t dltotal, curl_off_t dlnow, curl_off_t ultotal, curl_off_t ulnow) {
    if (dltotal <= 0) return 0;
    
    static auto lastUpdate = std::chrono::steady_clock::now();
    static curl_off_t lastBytes = 0;
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - lastUpdate);
    
    if (duration.count() >= 100) {
        struct winsize w;
        ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
        int termWidth = w.ws_col;
        
        int progressBarWidth = std::min(50, termWidth - 40);
        int progress = static_cast<int>((dlnow * 100.0) / dltotal);
        
        double speed = (dlnow - lastBytes) * 1000.0 / duration.count();
        double speedMB = speed / 1024.0 / 1024.0;
        
        double downloadedMB = dlnow / 1024.0 / 1024.0;
        double totalMB = dltotal / 1024.0 / 1024.0;
        
        std::stringstream ss;
        ss << "\r[";
        for (int i = 0; i < progressBarWidth; i++) {
            if (i < (progress * progressBarWidth) / 100) {
                ss << "=";
            } else if (i == (progress * progressBarWidth) / 100) {
                ss << ">";
            } else {
                ss << " ";
            }
        }
        ss << "] " << std::fixed << std::setprecision(2) 
           << downloadedMB << "MB/" << totalMB << "MB @ " 
           << speedMB << "MB/s";
        
        std::string output = ss.str();
        
        if (output.length() > static_cast<size_t>(termWidth)) {
            output = output.substr(0, termWidth - 3) + "...";
        }
        
        std::cout << "\033[2K" << output << std::flush;
        
        lastUpdate = now;
        lastBytes = dlnow;
    }
    
    return 0;
} 