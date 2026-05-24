#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <objidl.h> // IStream ve PROPID tanımlarının düzgün yüklenmesi için gerekli
#include <gdiplus.h>
#include <vector>
#include <string>
#include <iostream>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")

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
std::vector<char> CaptureScreenJPEG() {
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

    Gdiplus::GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken; // ULONG_ptr yerine standart ULONG_PTR yapıldı
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
    serverAddr.sin_port = htons(8080); // Sunucu portu: 8080

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
    std::cout << "Tarayicinizdan http://localhost:8080 veya http://<BILGISAYAR-IP-ADRESI>:8080 adresine gidin.\n";

    while (true) {
        SOCKET clientSocket = accept(listenSocket, NULL, NULL);
        if (clientSocket == INVALID_SOCKET) continue;

        char recvbuf[1024];
        int iResult = recv(clientSocket, recvbuf, sizeof(recvbuf) - 1, 0);
        if (iResult > 0) {
            recvbuf[iResult] = '\0';
            std::string request(recvbuf);

            // Ana sayfa isteği (HTML ve Kontrol Arayüzü)
            if (request.find("GET / ") != std::string::npos || request.find("GET /index.html") != std::string::npos) {
                std::string html = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: close\r\n\r\n"
                    "<!DOCTYPE html>"
                    "<html>"
                    "<head>"
                    "<title>PC Ekran Izleme</title>"
                    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
                    "<style>"
                    "body { font-family: sans-serif; text-align: center; background: #1e1e1e; color: #fff; margin: 0; padding: 10px; }"
                    "img { max-width: 100%; height: auto; border: 2px solid #444; margin-top: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.5); }"
                    "button { padding: 12px 24px; font-size: 16px; margin: 10px; cursor: pointer; background: #007bff; color: white; border: none; border-radius: 5px; transition: 0.2s; }"
                    "button:hover { background: #0056b3; }"
                    "#toggleAuto { background: #dc3545; }"
                    "#toggleAuto.stopped { background: #28a745; }"
                    "</style>"
                    "</head>"
                    "<body>"
                    "<h1>PC Ekran Goruntusu (Canli)</h1>"
                    "<button id=\"refreshBtn\">Manuel Yenile</button>"
                    "<button id=\"toggleAuto\">Otomatik Yenilemeyi Durdur</button>"
                    "<br>"
                    "<img id=\"screen\" src=\"/screenshot\" alt=\"Ekran Goruntusu Yukleniyor...\">"
                    "<script>"
                    "const img = document.getElementById('screen');"
                    "const refreshBtn = document.getElementById('refreshBtn');"
                    "const toggleBtn = document.getElementById('toggleAuto');"
                    "let intervalId = null;"
                    "function updateImg() {"
                    "  img.src = '/screenshot?t=' + Date.now();"
                    "}"
                    "refreshBtn.addEventListener('click', updateImg);"
                    "function startAuto() {"
                    "  intervalId = setInterval(updateImg, 1000);" // 1 saniyede bir yeniler
                    "  toggleBtn.textContent = 'Otomatik Yenilemeyi Durdur';"
                    "  toggleBtn.className = '';"
                    "}"
                    "function stopAuto() {"
                    "  clearInterval(intervalId);"
                    "  intervalId = null;"
                    "  toggleBtn.textContent = 'Otomatik Yenilemeyi Baslat';"
                    "  toggleBtn.className = 'stopped';"
                    "}"
                    "toggleBtn.addEventListener('click', () => {"
                    "  if (intervalId) { stopAuto(); } else { startAuto(); }"
                    "});"
                    "startAuto();" 
                    "</script>"
                    "</body>"
                    "</html>";
                send(clientSocket, html.c_str(), (int)html.length(), 0);
            } 
            // Ekran görüntüsü görsel isteği
            else if (request.find("GET /screenshot") != std::string::npos) {
                std::vector<char> jpegData = CaptureScreenJPEG();
                std::string headers = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: image/jpeg\r\n"
                    "Content-Length: " + std::to_string(jpegData.size()) + "\r\n"
                    "Connection: close\r\n\r\n";
                
                send(clientSocket, headers.c_str(), (int)headers.length(), 0);
                if (!jpegData.empty()) {
                    int totalSent = 0;
                    int size = (int)jpegData.size();
                    while (totalSent < size) {
                        int sent = send(clientSocket, jpegData.data() + totalSent, size - totalSent, 0);
                        if (sent == SOCKET_ERROR) break;
                        totalSent += sent;
                    }
                }
            }
            else {
                std::string response = "HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(clientSocket, response.c_str(), (int)response.length(), 0);
            }
        }
        closesocket(clientSocket);
    }

    closesocket(listenSocket);
    WSACleanup();
    return 0;
}
