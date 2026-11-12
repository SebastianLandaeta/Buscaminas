# Buscaminas de consola

Juego de Buscaminas escrito en C para terminales de Windows y POSIX. No requiere
dependencias externas.

## Requisitos

- Un compilador de C compatible con C99, como GCC.
- Una terminal interactiva. En POSIX, debe aceptar secuencias ANSI para limpiar
  la pantalla; en Windows, el programa espera una consola de Windows.

## Compilar

### Windows (MinGW-w64)

Desde PowerShell o CMD, en la carpeta del proyecto:

```powershell
gcc -std=c99 -Wall -Wextra -pedantic buscaminas.c -o buscaminas.exe
```

### Linux y macOS

```sh
gcc -std=c99 -Wall -Wextra -pedantic buscaminas.c -o buscaminas
```

## Ejecutar

En Windows:

```powershell
.\buscaminas.exe
```

En Linux o macOS:

```sh
./buscaminas
```

## Cómo jugar

- En el menú principal, usa las flechas y `Enter` para seleccionar **Jugar** o
  **Salir**.
- Elige una dificultad:
  - **Principiante:** 8 x 8, 10 minas.
  - **Intermedio:** 16 x 16, 40 minas.
  - **Experto:** 16 x 30, 99 minas.
- Durante la partida, muévete por el tablero con las flechas.
- Pulsa `a` o `Enter` para abrir la casilla seleccionada.
- Pulsa `b` para colocar o quitar una bandera.
- La primera casilla que abras estará libre de minas.
- Al ganar o perder, el tablero mostrará el resultado. Pulsa cualquier tecla
  para regresar al menú principal.

## Símbolos del tablero

| Símbolo | Significado |
| --- | --- |
| `>` | Cursor en la casilla seleccionada |
| `#` | Casilla cerrada |
| `B` | Casilla marcada con una bandera |
| `.` | Casilla abierta sin minas adyacentes |
| `1`–`8` | Cantidad de minas en las casillas vecinas |
| `*` | Mina |
