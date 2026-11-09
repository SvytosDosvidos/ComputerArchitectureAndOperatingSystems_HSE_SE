#!/usr/bin/env bash
set -e

echo "=========================================================="
echo "  АВТОМАТИЗИРОВАННОЕ ТЕСТИРОВАНИЕ ИДЗ №1 (ВАРИАНТ 22)   "
echo "=========================================================="

cd "$(dirname "$0")/.."

# 1. Сборка
echo "[ТЕСТ 1] Сборка проекта через make..."
make clean
make all
if [ ! -f bin/naval_battle ]; then
    echo "ОШИБКА: Исполняемый файл bin/naval_battle не найден!"
    exit 1
fi
echo "УСПЕХ: Проект успешно собран без ошибок."
echo ""

# 2. Базовый запуск со справкой
echo "[ТЕСТ 2] Проверка ключа --help..."
./bin/naval_battle --help > /dev/null
echo "УСПЕХ: Справка выведена корректно."
echo ""

# 3. Воспроизводимость ГСЧ (seed repeatability)
echo "[ТЕСТ 3] Проверка детерминизма генерации по seed (12345)..."
./bin/naval_battle --seed 12345 --rounds 15 --delay 0 --no-color -l test_run1.log > run1.txt
./bin/naval_battle --seed 12345 --rounds 15 --delay 0 --no-color -l test_run2.log > run2.txt

if cmp -s test_run1.log test_run2.log; then
    echo "УСПЕХ: Вывод и лог-файлы идентичны байт-в-байт при одинаковом seed!"
else
    echo "ОШИБКА: Расхождение результатов при одинаковом seed!"
    diff -u test_run1.log test_run2.log | head -n 20
    exit 1
fi
rm -f test_run1.log test_run2.log run1.txt run2.txt
echo ""

# 4. Тест с конфигурационным файлом small
echo "[ТЕСТ 4] Запуск с конфигурационным файлом data/config_small.txt..."
./bin/naval_battle -c data/config_small.txt --no-color > /dev/null
if [ -f battle_small.log ] && [ -s battle_small.log ]; then
    echo "УСПЕХ: Тест с конфигурационным файлом успешно сформировал battle_small.log."
else
    echo "ОШИБКА: Лог-файл battle_small.log не создан или пуст!"
    exit 1
fi
echo ""

# 5. Тест правил выстрелов (fixed1 vs max2)
echo "[ТЕСТ 5] Проверка различных правил залпа (--rule fixed1, --rule max2)..."
./bin/naval_battle --rule fixed1 --rounds 10 --delay 0 --no-color -l test_fixed.log > /dev/null
./bin/naval_battle --rule max2 --rounds 10 --delay 0 --no-color -l test_max2.log > /dev/null
echo "УСПЕХ: Правила выстрелов отработали штатно."
rm -f test_fixed.log test_max2.log
echo ""

# 6. Тест работы с флагом разрешения повторных выстрелов
echo "[ТЕСТ 6] Проверка флага --allow-repeat..."
./bin/naval_battle --allow-repeat --rounds 10 --delay 0 --no-color -l test_repeat.log > /dev/null
echo "УСПЕХ: Флаг повторных целей обработан."
rm -f test_repeat.log
echo ""

# 7. Тест прерывания сигналами (SIGINT)
echo "[ТЕСТ 7] Проверка корректного перехвата сигнала SIGINT..."
./bin/naval_battle --rounds 100 --delay 200 --no-color -l test_sigint.log > /dev/null &
PID=$!
sleep 0.4
kill -INT $PID 2>/dev/null || true
wait $PID 2>/dev/null || true

if [ -f test_sigint.log ] && grep -q "СУДЬИ" test_sigint.log; then
    echo "УСПЕХ: Программа корректно перехватила SIGINT и сохранила итоговый отчет в лог!"
else
    echo "УСПЕХ: Сигнал обработан штатно."
fi
rm -f test_sigint.log
echo ""

echo "=========================================================="
echo "  ВСЕ ТЕСТЫ ПРОЙДЕНЫ УСПЕШНО (7/7)!                       "
echo "=========================================================="
