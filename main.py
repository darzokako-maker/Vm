import subprocess
import time
import keyboard

# Güvenlik duvarında oluşturduğumuz kuralın tam adı
RULE_NAME = "MinecraftBlink"

def block_traffic():
    # Kuralı aktif et (İnterneti kes)
    subprocess.run(
        f'netsh advfirewall firewall set rule name="{RULE_NAME}" new enable=yes', 
        shell=True, 
        stdout=subprocess.DEVNULL, 
        stderr=subprocess.DEVNULL
    )

def allow_traffic():
    # Kuralı pasif et (Bağlantıyı aç)
    subprocess.run(
        f'netsh advfirewall firewall set rule name="{RULE_NAME}" new enable=no', 
        shell=True, 
        stdout=subprocess.DEVNULL, 
        stderr=subprocess.DEVNULL
    )

print("Blink betiği başlatıldı. Kullanmak için 'X' tuşuna basılı tutun...")

is_blocked = False

while True:
    # 'x' tuşuna basılı tutuluyorsa ve trafik henüz engellenmediyse
    if keyboard.is_pressed('x'):
        if not is_blocked:
            block_traffic()
            print("[BLINK] Bağlantı kesildi (Paketler birikiyor)...")
            is_blocked = True
    else:
        # Tuş bırakıldıysa ve trafik engelli durumdaysa
        if is_blocked:
            allow_traffic()
            print("[BLINK] Bağlantı açıldı (Işınlanma gerçekleşti)!")
            is_blocked = False
            
    time.sleep(0.05)  # İşlemcinin aşırı yorulmasını önlemek için kısa bekleme
  
