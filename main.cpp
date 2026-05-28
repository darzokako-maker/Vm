#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <objidl.h> 
#include <gdiplus.h>
#include <vector>
#include <string>
#include <iostream>
#include <sstream>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")

// Tarayıcıdan gelen URL kodlu metinleri çözen yardımcı fonksiyon
std::wstring UrlDecodeAndConvert(const std::string& str) {
    std::string decoded = "";
    for (size_t pos = 0; pos < str.length(); pos++) {
        if (str[pos] == '%') {
            if (pos + 2 < str.length()) {
                int value;
                std::istringstream hexStream(str.substr(pos + 1, 2));
                if (hexStream >> std::hex >> value) {
                    decoded += static_cast<char>(value);
                    pos += 2;
                }
            }
        } else if (str[pos] == '+') {
            decoded += ' ';
        } else {
            decoded += str[pos];
        }
    }
    
    int size_needed = MultiByteToWideChar(CP_UTF8, 0, &decoded[0], (int)decoded.size(), NULL, 0);
    std::wstring wstrTo(size_needed, 0);
    MultiByteToWideChar(CP_UTF8, 0, &decoded[0], (int)decoded.size(), &wstrTo[0], size_needed);
    return wstrTo;
}

// Uzaktan gönderilen metni PC'de simüle eden fonksiyon
void SimulateKeyboardInput(const std::wstring& text) {
    for (wchar_t ch : text) {
        INPUT input = { 0 };
        input.type = INPUT_KEYBOARD;
        input.ki.dwFlags = KEYEVENTF_UNICODE;
        input.ki.wScan = ch;
        
        SendInput(1, &input, sizeof(INPUT));
        
        input.ki.dwFlags |= KEYEVENTF_KEYUP;
        SendInput(1, &input, sizeof(INPUT));
    }
}

// Enter, Backspace gibi özel tuşları simüle eden fonksiyon
void SimulateSpecialKey(WORD vk) {
    INPUT inputs[2] = { 0 };
    inputs[0].type = INPUT_KEYBOARD;
    inputs[0].ki.wVk = vk;
    
    inputs[1].type = INPUT_KEYBOARD;
    inputs[1].ki.wVk = vk;
    inputs[1].ki.dwFlags = KEYEVENTF_KEYUP;
    
    SendInput(2, inputs, sizeof(INPUT));
}

// Fare imlecini ekran görüntüsünün üzerine çizen fonksiyon
void DrawMouseCursor(HDC hDC) {
    CURSORINFO cursorInfo = { 0 };
    cursorInfo.cbSize = sizeof(CURSORINFO);
    if (GetCursorInfo(&cursorInfo)) {
        if (cursorInfo.flags == CURSOR_SHOWING) {
            ICONINFO iconInfo = { 0 };
            if (GetIconInfo(cursorInfo.hCursor, &iconInfo)) {
                int x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
                int y1 = GetSystemMetrics(SM_YVIRTUALSCREEN);
                
                int x = cursorInfo.ptScreenPos.x - x1 - iconInfo.xHotspot;
                int y = cursorInfo.ptScreenPos.y - y1 - iconInfo.yHotspot;
                
                DrawIcon(hDC, x, y, cursorInfo.hCursor);
                
                if (iconInfo.hbmMask) DeleteObject(iconInfo.hbmMask);
                if (iconInfo.hbmColor) DeleteObject(iconInfo.hbmColor);
            }
        }
    }
}

// GDI+ için JPEG format kodlayıcısını bulan yardımcı fonksiyon
int GetEncoderClsid(const WCHAR* format, CLSID* pClsid) {
    UINT num = 0;          
    UINT size = 0;         
    Gdiplus::GetImageEncodersSize(&num, &size);
    if (size == 0) return -1;  

    Gdiplus::ImageCodecInfo* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
    if (pImageCodecInfo == NULL) return -1;  

    Gdiplus::GetImageEncoders(num, size, pImageCodecInfo);

    for (UINT j = 0; j < num; ++j) {
        if (wcscmp(pImageCodecInfo[j].MimeType, format) == 0) {
            *pClsid = pImageCodecInfo[j].Clsid;
            free(pImageCodecInfo);
            return j;  
        }
    }

    free(pImageCodecInfo);
    return -1;  
}

// Ekran görüntüsünü yakalayıp JPEG formatında bir vektöre yazan fonksiyon
std::vector<char> CaptureScreenJPEG(bool drawCursor) {
    std::vector<char> buffer;
    
    int x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y1 = GetSystemMetrics(SM_YVIRTUALSCREEN);
    int width = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int height = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HDC hScreenDC = GetDC(NULL);
    HDC hMemoryDC = CreateCompatibleDC(hScreenDC);
    HBITMAP hBitmap = CreateCompatibleBitmap(hScreenDC, width, height);
    HBITMAP hOldBitmap = (HBITMAP)SelectObject(hMemoryDC, hBitmap);

    BitBlt(hMemoryDC, 0, 0, width, height, hScreenDC, x1, y1, SRCCOPY);

    if (drawCursor) {
        DrawMouseCursor(hMemoryDC);
    }

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken; 
    Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);

    {
        Gdiplus::Bitmap bitmap(hBitmap, NULL);
        CLSID clsid;
        if (GetEncoderClsid(L"image/jpeg", &clsid) != -1) {
            IStream* pStream = NULL;
            if (SUCCEEDED(CreateStreamOnHGlobal(NULL, TRUE, &pStream))) {
                if (SUCCEEDED(bitmap.Save(pStream, &clsid, NULL))) {
                    HGLOBAL hg = NULL;
                    if (SUCCEEDED(GetHGlobalFromStream(pStream, &hg))) {
                        size_t bufSize = GlobalSize(hg);
                        void* pBuffer = GlobalLock(hg);
                        if (pBuffer) {
                            buffer.assign((char*)pBuffer, (char*)pBuffer + bufSize);
                            GlobalUnlock(hg);
                        }
                    }
                }
                pStream->Release();
            }
        }
    }

    Gdiplus::GdiplusShutdown(gdiplusToken);

    SelectObject(hMemoryDC, hOldBitmap);
    DeleteObject(hBitmap);
    DeleteDC(hMemoryDC);
    ReleaseDC(NULL, hScreenDC);

    return buffer;
}

// Tıklama simülasyonu
void SimulateClick(double rx, double ry) {
    int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y1 = GetSystemMetrics(SM_YVIRTUALSCREEN);

    int targetX = x1 + static_cast<int>(rx * screenWidth);
    int targetY = y1 + static_cast<int>(ry * screenHeight);

    SetCursorPos(targetX, targetY);

    INPUT inputs[2] = {};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = MOUSEEVENTF_LEFTDOWN;
    
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = MOUSEEVENTF_LEFTUP;

    SendInput(2, inputs, sizeof(INPUT));
}

int main() {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "Winsock baslatilamadi.\n";
        return 1;
    }

    SOCKET listenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listenSocket == INVALID_SOCKET) {
        std::cerr << "Soket olusturulamadi.\n";
        WSACleanup();
        return 1;
    }

    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port = htons(8080); 

    if (bind(listenSocket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Port baglanamadi (Port 8080 kullanimda olabilir).\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    if (listen(listenSocket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Dinleme hatasi.\n";
        closesocket(listenSocket);
        WSACleanup();
        return 1;
    }

    std::cout << "Sunucu baslatildi. Port: 8080\n";

    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) continue;

        char recvbuf[1024];
        int iResult = recv(clientSocket, recvbuf, sizeof(recvbuf) - 1, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            std::string request(recvbuf);

            // Klavye komutlarını işleme
            if (request.find("GET /keyboard") != std::string::npos) {
                size_t text_pos = request.find("text=");
                size_t key_pos = request.find("key=");
                
                if (text_pos != std::string::npos) {
                    size_t space_pos = request.find(" ", text_pos);
                    std::string encoded_text = request.substr(text_pos + 5, space_pos - (text_pos + 5));
                    std::wstring decoded_text = UrlDecodeAndConvert(encoded_text);
                    SimulateKeyboardInput(decoded_text);
                }
                else if (key_pos != std::string::npos) {
                    size_t space_pos = request.find(" ", key_pos);
                    std::string key_name = request.substr(key_pos + 4, space_pos - (key_pos + 4));
                    
                    if (key_name == "enter") {
                        SimulateSpecialKey(VK_RETURN);
                    } else if (key_name == "backspace") {
                        SimulateSpecialKey(VK_BACK);
                    }
                }

                std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(clientSocket, response.c_str(), (int)response.length(), 0);
            }
            // Tıklama İsteği İşleme
            else if (request.find("GET /click") != std::string::npos) {
                try {
                    size_t rx_pos = request.find("rx=");
                    size_t ry_pos = request.find("ry=");
                    if (rx_pos != std::string::npos && ry_pos != std::string::npos) {
                        size_t amp_pos = request.find("&", rx_pos);
                        std::string rx_str = request.substr(rx_pos + 3, amp_pos - (rx_pos + 3));
                        
                        size_t space_pos = request.find(" ", ry_pos);
                        std::string ry_str = request.substr(ry_pos + 3, space_pos - (ry_pos + 3));
                        
                        double rx = std::stod(rx_str);
                        double ry = std::stod(ry_str);

                        if (rx >= 0.0 && rx <= 1.0 && ry >= 0.0 && ry <= 1.0) {
                            SimulateClick(rx, ry);
                        }
                    }
                } catch (...) {}
                std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(clientSocket, response.c_str(), (int)response.length(), 0);
            }
            // Ana sayfa isteği
            else if (request.find("GET / ") != std::string::npos || request.find("GET /index.html") != std::string::npos) {
                std::string html = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: close\r\n\r\n"
                    "<!DOCTYPE html>"
                    "<html>"
                    "<head>"
                    "<title>PC Ekran Izleme ve Kontrol (5 FPS)</title>"
                    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
                    "<style>"
                    "body { font-family: sans-serif; text-align: center; background: #1
