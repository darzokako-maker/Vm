#include <jni.h>
#include <pthread.h>
#include <unistd.h>
#include <android/log.h>
#include <GLES2/gl2.h>
#include <EGL/egl.h>
#include <stdint.h>
#include <string.h>
#include <sys/mman.h>
#include <stdlib.h>
#include "dobby.h"

#define LOG_TAG "PolyfieldStealth"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)

// Global Durumlar
bool isMenuOpen = true;
bool isSpeedHackActive = false;
bool isInfiniteAmmoActive = false;

int screenWidth = 1920;
int screenHeight = 1080;

uintptr_t il2cppBase = 0;

// Orijinal Fonksiyon Pointer'lari (Hook sonrasi orijinalleri cagirmak icin)
EGLBoolean (*orig_eglSwapBuffers)(EGLDisplay dpy, EGLSurface surface) = nullptr;
float (*orig_get_SpeedMultiplier)(void* instance) = nullptr;
int32_t (*orig_get_CurrentAmmo)(void* instance) = nullptr;

// Bellek Sayfa Korumasini Kaldirma Yardımcısı
void MakeWritable(uintptr_t address, size_t size) {
    size_t pageSize = sysconf(_SC_PAGESIZE);
    uintptr_t pageStart = address & ~(pageSize - 1);
    mprotect((void*)pageStart, size + (address - pageStart), PROT_READ | PROT_WRITE | PROT_EXEC);
}

// Bellekten Modül Base Adresi Bulucu (/proc/self/maps okur)
uintptr_t GetBaseAddress(const char* libraryName) {
    uintptr_t baseAddress = 0;
    char line[512];
    FILE* maps = fopen("/proc/self/maps", "rt");
    if (maps != NULL) {
        while (fgets(line, sizeof(line), maps)) {
            if (strstr(line, libraryName)) {
                baseAddress = strtoul(line, NULL, 16);
                break;
            }
        }
        fclose(maps);
    }
    return baseAddress;
}

// Kendi Renkli Dikdörtgen Çizim Motorumuz (OpenGL ES 2.0)
void DrawMenuRect(float x, float y, float width, float height, float r, float g, float b, float a) {
    float glX = (x / screenWidth) * 2.0f - 1.0f;
    float glY = -((y / screenHeight) * 2.0f - 1.0f);
    float glW = (width / screenWidth) * 2.0f;
    float glH = (height / screenHeight) * 2.0f;

    GLfloat vertices[] = {
        glX,        glY,
        glX + glW,  glY,
        glX,        glY - glH,
        glX + glW,  glY - glH
    };

    // Basit renk baglama (Dinamik kanal)
    glColor4f(r, g, b, a);
    
    glEnableClientState(GL_VERTEX_ARRAY);
    glVertexPointer(2, GL_FLOAT, 0, vertices);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glDisableClientState(GL_VERTEX_ARRAY);
}

// --- HOOK: EKRAN YENİLEME DÖNGÜSÜ (EGL SWAP BUFFERS) ---
EGLBoolean hook_eglSwapBuffers(EGLDisplay dpy, EGLSurface surface) {
    eglQuerySurface(dpy, surface, EGL_WIDTH, &screenWidth);
    eglQuerySurface(dpy, surface, EGL_HEIGHT, &screenHeight);

    if (isMenuOpen) {
        // Ana Panel Arka Plani (Koyu Tema)
        DrawMenuRect(100, 100, 500, 420, 0.12f, 0.12f, 0.12f, 0.90f);
        
        // Baslik Cubugu (Uyumlu Mavi)
        DrawMenuRect(100, 100, 500, 45, 0.18f, 0.52f, 0.89f, 1.0f);

        // BUTON 1: HIZ HİLESİ (Aktifken Yesil, Pasifken Kirmizi)
        if (isSpeedHackActive) {
            DrawMenuRect(130, 180, 440, 55, 0.15f, 0.68f, 0.37f, 1.0f);
        } else {
            DrawMenuRect(130, 180, 440, 55, 0.75f, 0.22f, 0.16f, 1.0f);
        }

        // BUTON 2: SINIRSIZ MERMİ
        if (isInfiniteAmmoActive) {
            DrawMenuRect(130, 260, 440, 55, 0.15f, 0.68f, 0.37f, 1.0f);
        } else {
            DrawMenuRect(130, 260, 440, 55, 0.75f, 0.22f, 0.16f, 1.0f);
        }
        
        // Kapatma Alani Butonu (Menuyu Gizlemek/Acmak Icin Kucuk Kare)
        DrawMenuRect(100, 360, 500, 40, 0.3f, 0.3f, 0.3f, 1.0f);
    } else {
        // Menu kapaliyken ekranda duracak kucuk "AC" butonu
        DrawMenuRect(10, 10, 90, 40, 0.18f, 0.52f, 0.89f, 0.70f);
    }

    return orig_eglSwapBuffers(dpy, surface);
}

// --- HOOK: OYUN MEKANİKLERİ ---

// Hiz Hilesi Hook'u (ActvityController.get_SpeedMultiplier)
float hook_get_SpeedMultiplier(void* instance) {
    if (isSpeedHackActive) {
        return 3.0f; // Hiz carpani aktifken 3 kat hizli kosar
    }
    return orig_get_SpeedMultiplier(instance);
}

// Mermi Hilesi Hook'u (Weapon.get_CurrentAmmo)
int32_t hook_get_CurrentAmmo(void* instance) {
    if (isInfiniteAmmoActive) {
        return 99; // Mermiyi sürekli 99'a esitler
    }
    return orig_get_CurrentAmmo(instance);
}

// --- DOKUNMA / TIKLAMA YÖNETİMİ ---
// Bu fonksiyonu dilersen oyuna Smali enjeksiyonu yaparken MotionEvent uzerinden besleyebilirsin
void MenuOnTouchHandler(float tx, float ty, int action) {
    if (action == 1) { // ACTION_UP (Dokunup cekme anı)
        if (isMenuOpen) {
            // Hiz Butonu Alani: X(130-570), Y(180-235)
            if (tx >= 130 && tx <= 570 && ty >= 180 && ty <= 235) {
                isSpeedHackActive = !isSpeedHackActive;
                LOGI("Speedhack Durumu: %d", isSpeedHackActive);
            }
            // Mermi Butonu Alani: X(130-570), Y(260-315)
            if (tx >= 130 && tx <= 570 && ty >= 260 && ty <= 315) {
                isInfiniteAmmoActive = !isInfiniteAmmoActive;
                LOGI("Sirsiz Mermi Durumu: %d", isInfiniteAmmoActive);
            }
            // Menuyu Kapatma Cubugu Alani
            if (tx >= 100 && tx <= 600 && ty >= 360 && ty <= 400) {
                isMenuOpen = false;
            }
        } else {
            // Menu kapaliyken sol ustteki "AC" butonuna tiklama kontrolü
            if (tx >= 10 && tx <= 100 && ty >= 10 && ty <= 50) {
                isMenuOpen = true;
            }
        }
    }
}

// Arka planda kütüphaneleri bekleyen ve Dobby ile Hook atan ana Thread
void* InjectionEngineThread(void*) {
    LOGI("Hile motoru arka planda baslatildi.");

    // 1. Oyunun Grafik Kütüphanesini Kancala
    uintptr_t eglBase = 0;
    while (eglBase == 0) {
        eglBase = GetBaseAddress("libEGL.so");
        usleep(100000);
    }
    void* eglSwapBuffers_addr = (void*)DobbySymbolResolver("libEGL.so", "eglSwapBuffers");
    if (eglSwapBuffers_addr) {
        DobbyHook(eglSwapBuffers_addr, (void*)hook_eglSwapBuffers, (void**)&orig_eglSwapBuffers);
        LOGI("Grafik dongusu (eglSwapBuffers) basariyla kancalandi!");
    }

    // 2. Oyunun Kendi Kod Kütüphanesini Bekle ve Kancala
    while (il2cppBase == 0) {
        il2cppBase = GetBaseAddress("libil2cpp.so");
        usleep(100000);
    }
    LOGI("libil2cpp.so yuklendi. Base: %p. Hileler aktif ediliyor...", (void*)il2cppBase);

    // Hız Hilesi Hook (Offset: 0x11855F8)
    DobbyHook((void*)(il2cppBase + 0x11855F8), (void*)hook_get_SpeedMultiplier, (void**)&orig_get_SpeedMultiplier);
    
    // Mermi Hilesi Hook (Offset: 0x11311F0)
    DobbyHook((void*)(il2cppBase + 0x11311F0), (void*)hook_get_CurrentAmmo, (void**)&orig_get_CurrentAmmo);

    LOGI("Tüm fonksiyon kancalari (Hooks) basariyla yerlestirildi.");
    return NULL;
}

// .so dosyası sisteme inject edildigi an calisan kurucu metod
__attribute__((constructor))
void MainStealthConstructor() {
    pthread_t thread;
    pthread_create(&thread, NULL, InjectionEngineThread, NULL);
}
