---
weight: 60
title: "mmap"
enableEmoji: true
# bookCollapseSection: true
---

# Виртуальная и физическая память

## Сегментация памяти

ОСи надо как-то обеспечить памятью все процессы.
Её для этого и создавали))

Одно из простых решений "влоб" - выдавать при старте одинаковые куски памяти.
Условно, первому процессу - с `0x0` до `0x1000000000`, второму -
с `0x2000000000` до `0x3000000000` ну и т.д.
Плюсы очевидны - формально проблема решена.
Минусы тоже очевидны - мы не можем "докинуть" памяти.

Однако мысль про "выдавать" правильная и его можно немного докрутить и оно
станет хорошим.
Память разбита на фреймы по 4096 байт.
При старте процесса ОСь выдает ему "неколько штук" фреймов памяти.
Когда процесс вызывает `malloc` / `new` операционка докидывает ему несколько
страниц, а после вызова `free` / `delete` забирает обратно.

За тем, какой фрейм относится к какому процессу - следит ОСь.


## Следите за руками

Сейчас будет магия.
Вот есть простой код:
```c
#include <stdio.h>
#include <unistd.h>

extern void _start();

int main() {
    printf("main: 0x%x\n", main);
    printf("_start: 0x%x\n", _start);

    usleep(10e+7);  // спим 10 секунд
}
```

Далее собираем без рандомизации адресов и (из-за этого) статически (`TODO: вставить ссылку, когда появится`)
```bash
gcc -fno-pic --static main.c
```

А теперь запускаем (в фоне `&`) нашу программу сразу несколько раз
```bash
[danila@archlinux /tmp/example]
$ ./a.out &
[1] 56627
main: 0x401805
_start: 0x4016e0
[danila@archlinux /tmp/example]
$ ./a.out &
[2] 56634
main: 0x401805
_start: 0x4016e0
[danila@archlinux /tmp/example]
$ ./a.out &
[3] 56641
main: 0x401805
_start: 0x4016e0
```

Адреса получились **одинаковые**.

> Ну конечно одинаковые! Это же одна и та же программа!

Нууууу... да, но это не поэтому)) Можете попробовать написать программу иначе или
добавить в неё что-нибудь.
Адреса не поменяются.

Да и ОСь не на столько умная.
Это мы можем сказать, что запускаем "одно и то же".
А ядру надо пройтись по 200+ (`ps -A | sort --reverse | wc -l`) запущенным
процессам ради того, чтоб _возможно_ найти такую же программу.
(короче так себе оптимизация)


## Виртуальное адресное пространство

В заголовке - разгадка фокуса.
Те "адреса", которые видит программа - это не настоящие адреса.

Виртуальное адресное пространство всое у каждого процесса.
Оно изолировано от остальных процессов.
Во время обращения (чтения / записи) адресс переводится из виртуального в физический.


## Страницы

- Физическая делится на **фреймы** одинакового размера (по 4096 байт в x86)
- Виртуальная память делится на **страницы** такого же размера
- В каждом виртуальном адресном пространстве каждой странице _может_ соответствовать какой-то фрейм

![vaddr](../../../sems/mmap/vaddr.png)

Запущенный процесс ничего не знает про остальные.
Во время написания программы мы почти никогда не думаем о других.
(спойлер - как начнем, всё равно будем взаимодействовать с ядром)
Виртуальное адресное пространство - один из механизмов, помогающих ОСи
поддерживать "легенду" о единственном процессе.


## Табличная адресация

С помощью 12 бит (`4096 = 2^12`) можно адресовать любую ячейку.
Это значит что самих страниц может быть `2^64 / 2^12 = 2^52` штук.
Т.к. на "страницу" в памяти тоже надо как-то ссылаться (очев через адресс), то
возникает мааааленькая проблема.
Если адрес страницы = `2^8` (размер регистра), то чтоб где-то хранить только
адреса самих страниц понадобится `2^64` байт памяти, а это примерно...
ооочень много... (`2^10` - кило, `2^20` - мега `2^30` - гига, `2^40` - тера)
я даже таких слов не знаю))


### Одна таблица

Поэтому надо придумать что-нибудь оптимальнее.
Для решения этой задачи и была придумана ~~хэшмапа~~ табличная адресация.

Таблица - это кусок памяти, в котором лежат адреса (по `8` байт).
Чтоб было проще, каждая таблица так же имеет размер `4096` байт.
Получается `4096 / 8 = 512` штук записей в каждой таблице.

Всего одной табличком можно адресовать 2 мегабайта памяти (`512 * 4096`).


### Многоуровневость

Страниц существует 4 уровня - первый, второй, третий, четвертый.
В табличке P1 (первого уровня) лежит адрес на страницу памяти.

В таблице P2 лежат указатели на таблицы P1.
В P3 - указатели на P2.
А в P4 - на P3.
Сам указатель на P4 лежит в регистре `cr3`.

![pages](../../../sems/mmap/pages.png)


### А где смещения хранятся

> Окей, хорошо.
> В таблице одного уровня хранятся указатели на следующие таблицы либо
> на страницы
> Как понять, какой из них нужен?

А вот эта информация хранится в указателе... Указатель 64бита побит на куски
`12 бит` + 4 раза по `9 бит`.

![pointer](../../../sems/mmap/pointer.png)

В каждом куске по `9 бит` содержится номер "адреса" в соответствующей странице
(`2^9 = 512` так что мы можем ссылаться на любой)

### Общая картинка

![pointer & pages](../../../sems/mmap/pointer_pages.png)

### Флаги

Помните, что страницы имеют размер 4K?
Так вот это правда)
И поэтому адреса таких страниц у нас выровнены по 4K, а это уже значит, что
младшие 12 бит (`2^12 = 4096`) у нас нули.

Так вот это - расточительство драгоценной памяти, поэтому там хранятся флаги.
Флаги отвечают за правад доступа к странице

(`PTE` - это `Page Table Entity` вроде...)
- `PTE_PRESENT` - флаг существования страницы (1 - да / 0 - нет)
- `PTE_WRITE` - разрешение на запись
- `PTE_USER` - доступность страницы из userspace-а
...


## COW

Выделение памяти - ~~дорогая~~ очень дорогая операция.
Чтоб снять часть работы, в ядре Linux есть оптимизация - `COW` aka
`Copy On Write`.

Если оба процесса шарят память между собой и используют её только для чтения,
то не имеет смысла "делать" из одного куска памяти - два куска памяти.

Сложности возникают, только когда один из процессов хочет в эту память
что-нибудь записать.
В этот момент страницы копируются, делаются изменения в таблицах, и
получается новый кусок памяти.


# mmap


## Мантра

Есть виртуальное адресное простарнство, есть страницы памяти, ...
И есть устройства с произвольным доступом - RAM / SSD / HDD / ...

> Мантра, с которой будем жить
>
> **выделение памяти есть отображение**

В ней больше смысла, чем кажется)
Есть виртуальное адресное пространство - `2^64` адресов, большая часть из
которых ни на что не ссылается (разыменование --> segfault)

Чтоб вместо segfault случилось что-то хорошее - надо задать отображение.
Ну т.е. сопоставить адрес (физический) настоящего устройства с адресов
в виртуальной памяти.


## Как выделять память

В linux мы выделяем страницы в памяти используя `mmap`

{{% details title="man mmap" open=false %}}
```bash
MMAP(3P)                  POSIX Programmer's Manual                 MMAP(3P)

PROLOG
       This manual page is part of the POSIX Programmer's Manual.  The Linux
       implementation  of this interface may differ (consult the correspond‐
       ing Linux manual page for details of Linux behavior), or  the  inter‐
       face may not be implemented on Linux.

NAME
       mmap — map pages of memory

SYNOPSIS
       #include <sys/mman.h>

       void *mmap(void *addr, size_t len, int prot, int flags,
           int fildes, off_t off);

DESCRIPTION
       The  mmap()  function  shall  establish  a mapping between an address
       space of a process and a memory object.

       The mmap() function shall be supported for the following  memory  ob‐
       jects:

        *  Regular files

        *  Shared memory objects

        *  Typed memory objects

       Support for any other type of file is unspecified
...
```
{{% /details %}}

У `mmap` довольно много аргументов, но они все простые

```c
void *mmap(
    void *addr,  // адресс, "в куда" отображать память

    size_t len,  // размер куска отображаемой памяти

    int prot,    // разрешения на этот кусок памяти
                 // PROT_NONE / PROT_READ / PROT_WRITE / PROT_EXEC

    int flags,   // прочие флаги и видимость для внешних процессов
                 // MAP_FIXED и MAP_SHARED / MAP_PRIVATE

    int fildes,  // файловый дескриптор, либо спец. значение
    off_t off,   // смещение относительно начала файла
);
```

Заметили что `fields` может быть файловым дескриптором?
Дело в том, что мы в "модели" виртуальной памяти вплоть до последнего
шага нигде не подразумевали, что мы работаем с RAM.
У RAM свой интерфейс.
У других устройств с произвольным доступом - свой (HDD / SSD).
Если унифицировать работу с ними (на уровне ядра) - то не будет разницы,
куда именно задавать отображение - в файл или в RAM.

На дисках есть файлы - это единственное, как мы можем работать с диском - а у
этих файлов есть имена)
А у RAM нету файлов и нету имен)
Отображения в RAM называются **анонимной памятью**