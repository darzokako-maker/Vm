#include <windows.h>
#include <iostream>
#include <thread>

// --- GİZLİ MASAÜSTÜ MOTORU ---
void CreateHiddenDesktop() {
    // 1. Yeni ve izole bir masaüstü oluştur
    HDESK hSafeDesktop = CreateDesktopA(
        "Yahya_Safe_Zone", 
        NULL, NULL, 0, 
        GENERIC_ALL, NULL
    );

    if (hSafeDesktop) {
        std::cout << "[+] Gizli Masaustu Olusturuldu!" << std::endl;
        
        // 2. Bu yeni masaüstüne geçiş yap (Anti-cheat burayı göremez)
        SwitchDesktop(hSafeDesktop);
        SetThreadDesktop(hSafeDesktop);

        // 3. Gizli masaüstü içinde bir "Dosya Gezgini" ve "Komut Satırı" başlat
        STARTUPINFOA si = { sizeof(si) };
        si.lpDesktop = (char*)"Yahya_Safe_Zone";
        PROCESS_INFORMATION pi;

        // Dosya ekleme/yönetme için Explorer benzeri bir yapı başlatır
        CreateProcessA("C:\\Windows\\explorer.exe", NULL, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi);
        
        std::cout << "[*] Dosya Gezgini gizli katmanda calisiyor." << std::endl;
        
        // Geri dönmek için bir süre bekle (veya bir tuş kombinasyonu ata)
        Sleep(20000); 
        
        // Ana masaüstüne geri dön
        HDESK hDefault = OpenDesktopA("Default", 0, FALSE, GENERIC_ALL);
        SwitchDesktop(hDefault);
        std::cout << "[!] Ana masaustune donuluyor..." << std::endl;
    }
}

int main() {
    SetConsoleTitleA("Yahya Stealth Desktop v1.0");
    std::cout << "--- Yahya Anti-Cheat Bypass Environment ---" << std::endl;
    std::cout << "[*] Gizli katman hazirlaniyor..." << std::endl;

    // Ayrı bir thread üzerinde başlat ki ana program çökmesin
    std::thread stealthThread(CreateHiddenDesktop);
    stealthThread.join();

    return 0;
}

