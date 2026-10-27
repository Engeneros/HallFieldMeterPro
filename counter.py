import keyboard
import time
import sys

# Фиксируем время старта
start_time = time.time()
count = 0

def on_key_press(event):
    global count
    count += 1

# Перехватываем все нажатия клавиш
keyboard.on_press(on_key_press)

print("Счетчик запущен в фоновом режиме.")
print("Для просмотра статистики нажмите Ctrl+C в этом терминале.")

try:
    while True:
        time.sleep(1)
except KeyboardInterrupt:
    # Вычисляем прошедшее время
    elapsed_time = time.time() - start_time
    
    # Переводим секунды в читаемый формат (минуты и секунды)
    minutes = int(elapsed_time // 60)
    seconds = int(elapsed_time % 60)
    
    # Считаем среднюю скорость (нажатий в минуту)
    cps = round((count / elapsed_time) * 60, 1) if elapsed_time > 0 else 0

    print("\n--- СТАТИСТИКА СЕССИИ ---")
    print(f"Время работы: {minutes} мин {seconds} сек")
    print(f"Всего нажатий: {count}")
    print(f"Средняя скорость: {cps} наж./мин")
    print("------------------------")
    sys.exit(0)
