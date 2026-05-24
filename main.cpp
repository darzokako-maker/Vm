#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <objidl.h> 
#include <gdiplus.h>
#include <vector>
#include <string>
#include <iostream>

#pragma comment(lib, "gdiplus.lib")
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "ole32.lib")

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
                
                // İmlecin tıklama ucunu (hotspot) hesaba katarak doğru konuma çiziyoruz
                int x = cursorInfo.ptScreenPos.x - x1 - iconInfo.xHotspot;
                int y = cursorInfo.ptScreenPos.y - y1 - iconInfo.yHotspot;
                
                DrawIcon(hDC, x, y, cursorInfo.hCursor);
                
                // Bellek sızıntısını önlemek için kaynakları temizliyoruz
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

    // Eğer kullanıcı imlecin görünmesini istiyorsa görüntünün üzerine çiz
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

// Telefondan gelen oranlara göre PC'de tıklama simülasyonu yapan fonksiyon
void SimulateClick(double rx, double ry) {
    int screenWidth = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    int screenHeight = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int x1 = GetSystemMetrics(SM_XVIRTUALSCREEN);
    int y1 = GetSystemMetrics(SM_YVIRTUALSCREEN);

    // Oransal koordinatı gerçek piksel koordinatına dönüştür
    int targetX = x1 + static_cast<int>(rx * screenWidth);
    int targetY = y1 + static_cast<int>(ry * screenHeight);

    // Fareyi hedef konuma taşı
    SetCursorPos(targetX, targetY);

    // Tıklama olayını gönder (Sol Bas ve Bırak)
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

            // Tıklama İsteği İşleme (Örn: GET /click?rx=0.234&ry=0.567)
            if (request.find("GET /click") != std::string::npos) {
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
                } catch (...) {
                    // Hatalı gelen parametreleri yok sayıyoruz
                }
                std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                send(clientSocket, response.c_str(), (int)response.length(), 0);
            }
            // Ana sayfa isteği (HTML, Butonlar ve JavaScript Arayüzü)
            else if (request.find("GET / ") != std::string::npos || request.find("GET /index.html") != std::string::npos) {
                std::string html = 
                    "HTTP/1.1 200 OK\r\n"
                    "Content-Type: text/html\r\n"
                    "Connection: close\r\n\r\n"
                    "<!DOCTYPE html>"
                    "<html>"
                    "<head>"
                    "<title>PC Ekran Izleme ve Kontrol</title>"
                    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
                    "<style>"
                    "body { font-family: sans-serif; text-align: center; background: #1e1e1e; color: #fff; margin: 0; padding: 10px; user-select: none; }"
                    "img { max-width: 100%; height: auto; border: 2px solid #444; margin-top: 10px; box-shadow: 0 4px 8px rgba(0,0,0,0.5); cursor: crosshair; touch-action: none; }"
                    "button { padding: 10px 20px; font-size: 15px; margin: 5px; cursor: pointer; background: #007bff; color: white; border: none; border-radius: 5px; transition: 0.2s; }"
                    "button:hover { background: #0056b3; }"
                    ".btn-off { background: #dc3545; }"
                    ".btn-off:hover { background: #bd2130; }"
                    ".btn-on { background: #28a745; }"
                    ".btn-on:hover { background: #218838; }"
                    "</style>"
                    "</head>"
                    "<body>"
                    "<h1>PC Uzaktan Kontrol Paneli</h1>"
                    "<button id=\"toggleCursor\" class=\"btn-on\">Imleci Goster: ACIK</button>"
                    "<button id=\"toggleControl\" class=\"btn-off\">Kontrol Modu: KAPALI</button>"
                    "<br>"
                    "<img id=\"screen\" src=\"/screenshot?show_cursor=1\" alt=\"Ekran Yukleniyor...\">"
                    "<script>"
                    "const img = document.getElementById('screen');"
                    "const cursorBtn = document.getElementById('toggleCursor');"
                    "const controlBtn = document.getElementById('toggleControl');"
                    "let showCursor = true;"
                    "let controlEnabled = false;"
                    "let intervalId = null;"
                    "function updateImg() {"
                    "  const cursorParam = showCursor ? '1' : '0';"
                    "  img.src = '/screenshot?show_cursor=' + cursorParam + '&t=' + Date.now();"
                    "}"
                    "cursorBtn.addEventListener('click', () => {"
                    "  showCursor = !showCursor;"
                    "  if (showCursor) {"
                    "    cursorBtn.textContent = 'Imleci Goster: ACIK';"
                    "    cursorBtn.className = 'btn-on';"
                    "  } else {"
                    "    cursorBtn.textContent = 'Imleci Goster: KAPALI';"
                    "    cursorBtn.className = 'btn-off';"
                    "  }"
                    "  updateImg();"
                    "});"
                    "controlBtn.addEventListener('click', () => {"
                    "  controlEnabled = !controlEnabled;"
                    "  if (controlEnabled) {"
                    "    controlBtn.textContent = 'Kontrol Modu: ACIK (Tıkla/Dokun)';"
                    "    controlBtn.className = 'btn-on';"
                    "  } else {"
                    "    controlBtn.textContent = 'Kontrol Modu: KAPALI';"
                    "    controlBtn.className = 'btn-off';"
                    "  }"
                    "});"
                    "img.addEventListener('pointerdown', (e) => {"
                    "  if (!controlEnabled) return;"
                    "  e.preventDefault();"
                    "  const rect = img.getBoundingClientRect();"
                    "  const x = e.clientX - rect.left;"
                    "  const y = e.clientY - rect.top;"
                    "  const rx = (x / rect.width).toFixed(4);"
                    "  const ry = (y / rect.height).toFixed(4);"
                    "  fetch(`/click?rx=${rx}&ry=${ry}`).then(() => {"
                    "    setTimeout(updateImg, 100); // Tiklama sonrasi ekrani hemen guncelle"
                    "  });"
                    "});"
                    "intervalId = setInterval(updateImg, 1000);" // 1 saniyede bir canli yenileme
                    "</script>"
                    "</body>"
                    "</html>";
                send(clientSocket, html.c_str(), (int)html.length(), 0);
            } 
            // Ekran görüntüsü görsel isteği
            else if (request.find("GET /screenshot") != std::string::npos) {
                bool drawCursor = true;
                if (request.find("show_cursor=0") != std::string::npos) {
                    drawCursor = false;
                }
                
                std::vector<char> jpegData = CaptureScreenJPEG(drawCursor);
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
