#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#else
#include <errno.h>
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

#define MAX_FILAS 16
#define MAX_COLUMNAS 30

typedef enum {
    TECLA_ARRIBA = 1000,
    TECLA_ABAJO,
    TECLA_IZQUIERDA,
    TECLA_DERECHA,
    TECLA_ENTER,
    TECLA_ESCAPE,
    TECLA_ERROR
} Tecla;

typedef struct {
    int mina;
    int adyacentes;
    int abierta;
    int bandera;
} Casilla;

typedef struct {
    int fila;
    int columna;
} Posicion;

typedef struct {
    Casilla casillas[MAX_FILAS][MAX_COLUMNAS];
    int filas;
    int columnas;
    int minas;
    int fila_cursor;
    int columna_cursor;
    int abiertas;
    int minas_colocadas;
    int estado;
} Partida;

enum {
    PARTIDA_EN_CURSO,
    PARTIDA_GANADA,
    PARTIDA_PERDIDA
};

#ifndef _WIN32
static struct termios terminal_original;
static int terminal_configurado = 0;
#endif

static int iniciar_terminal(void);
static int restaurar_terminal(void);
static int limpiar_pantalla(void);
static int leer_tecla(void);
static void mostrar_titulo(void);
static int mostrar_menu(const char *titulo, const char *opciones[], int cantidad);
static void inicializar_partida(Partida *partida, int filas, int columnas, int minas);
static void colocar_minas(Partida *partida, int fila_segura, int columna_segura);
static void abrir_casilla(Partida *partida);
static void expandir_casillas(Partida *partida, int fila, int columna);
static int dibujar_partida(const Partida *partida);
static int jugar(int filas, int columnas, int minas);

static int iniciar_terminal(void)
{
#ifdef _WIN32
    return 1;
#else
    struct termios temporal;

    if (tcgetattr(STDIN_FILENO, &terminal_original) == -1) {
        return 0;
    }

    temporal = terminal_original;
    temporal.c_lflag &= (tcflag_t) ~(ICANON | ECHO);
    temporal.c_cc[VMIN] = 1;
    temporal.c_cc[VTIME] = 0;

    if (tcsetattr(STDIN_FILENO, TCSANOW, &temporal) == -1) {
        return 0;
    }

    terminal_configurado = 1;
    return 1;
#endif
}

static int restaurar_terminal(void)
{
#ifdef _WIN32
    return 1;
#else
    if (terminal_configurado &&
        tcsetattr(STDIN_FILENO, TCSANOW, &terminal_original) == -1) {
        return 0;
    }

    terminal_configurado = 0;
    return 1;
#endif
}

static int limpiar_pantalla(void)
{
#ifdef _WIN32
    HANDLE consola = GetStdHandle(STD_OUTPUT_HANDLE);
    CONSOLE_SCREEN_BUFFER_INFO informacion;
    DWORD escritos;
    DWORD total;
    COORD inicio = { 0, 0 };

    if (consola == INVALID_HANDLE_VALUE ||
        !GetConsoleScreenBufferInfo(consola, &informacion)) {
        return 0;
    }

    total = (DWORD) informacion.dwSize.X * (DWORD) informacion.dwSize.Y;
    if (!FillConsoleOutputCharacterA(consola, ' ', total, inicio, &escritos) ||
        !FillConsoleOutputAttribute(consola, informacion.wAttributes, total,
                                    inicio, &escritos) ||
        !SetConsoleCursorPosition(consola, inicio)) {
        return 0;
    }

    return 1;
#else
    if (fputs("\033[2J\033[H", stdout) == EOF || fflush(stdout) == EOF) {
        return 0;
    }

    return 1;
#endif
}

#ifndef _WIN32
static int leer_byte(unsigned char *byte)
{
    ssize_t resultado;

    do {
        resultado = read(STDIN_FILENO, byte, 1);
    } while (resultado == -1 && errno == EINTR);

    return resultado == 1;
}

static int hay_byte_disponible(void)
{
    for (;;) {
        fd_set lectura;
        struct timeval espera;
        int resultado;

        FD_ZERO(&lectura);
        FD_SET(STDIN_FILENO, &lectura);
        espera.tv_sec = 0;
        espera.tv_usec = 50000;
        resultado = select(STDIN_FILENO + 1, &lectura, NULL, NULL, &espera);

        if (resultado == -1 && errno == EINTR) {
            continue;
        }
        return resultado > 0;
    }
}
#endif

static int leer_tecla(void)
{
#ifdef _WIN32
    int caracter = _getch();

    if (caracter == 0 || caracter == 224) {
        switch (_getch()) {
        case 72: return TECLA_ARRIBA;
        case 80: return TECLA_ABAJO;
        case 75: return TECLA_IZQUIERDA;
        case 77: return TECLA_DERECHA;
        default: return 0;
        }
    }

    if (caracter == '\r' || caracter == '\n') {
        return TECLA_ENTER;
    }
    if (caracter == 27) {
        return TECLA_ESCAPE;
    }

    return caracter;
#else
    unsigned char caracter;

    if (!leer_byte(&caracter)) {
        return TECLA_ERROR;
    }

    if (caracter == '\n' || caracter == '\r') {
        return TECLA_ENTER;
    }
    if (caracter != 27) {
        return (int) caracter;
    }

    /* Las flechas llegan como secuencias ANSI que comienzan con Escape. */
    if (!hay_byte_disponible()) {
        return TECLA_ESCAPE;
    }
    if (!leer_byte(&caracter) || (caracter != '[' && caracter != 'O')) {
        return TECLA_ESCAPE;
    }
    if (!hay_byte_disponible() || !leer_byte(&caracter)) {
        return TECLA_ESCAPE;
    }

    switch (caracter) {
    case 'A': return TECLA_ARRIBA;
    case 'B': return TECLA_ABAJO;
    case 'D': return TECLA_IZQUIERDA;
    case 'C': return TECLA_DERECHA;
    default: return TECLA_ESCAPE;
    }
#endif
}

static void mostrar_titulo(void)
{
    puts("############################################################");
    puts(" ####  #   # ##### #####  ###  #   # ##### #   #  ###  #####");
    puts(" #   # #   # #     #     #   # ## ##   #   ##  # #   # #");
    puts(" ####  #   # ##### #     ##### # # #   #   # # # ##### #####");
    puts(" #   # #   #     # #     #   # #   #   #   #  ## #   #     #");
    puts(" ####  ##### ##### ##### #   # #   # ##### #   # #   # #####");
    puts("############################################################");
}

static int mostrar_menu(const char *titulo, const char *opciones[], int cantidad)
{
    int seleccion = 0;

    for (;;) {
        int tecla;
        int indice;

        if (!limpiar_pantalla()) {
            return -1;
        }

        mostrar_titulo();
        printf("\n%s\n\n", titulo);
        for (indice = 0; indice < cantidad; ++indice) {
            printf("%s %s\n", indice == seleccion ? ">" : " ", opciones[indice]);
        }
        puts("\nUsa las flechas y Enter para seleccionar.");
        if (fflush(stdout) == EOF) {
            return -1;
        }

        tecla = leer_tecla();
        if (tecla == TECLA_ERROR) {
            return -1;
        }
        if (tecla == TECLA_ARRIBA) {
            seleccion = (seleccion + cantidad - 1) % cantidad;
        } else if (tecla == TECLA_ABAJO) {
            seleccion = (seleccion + 1) % cantidad;
        } else if (tecla == TECLA_ENTER) {
            return seleccion;
        }
    }
}

static void inicializar_partida(Partida *partida, int filas, int columnas, int minas)
{
    memset(partida, 0, sizeof(*partida));
    partida->filas = filas;
    partida->columnas = columnas;
    partida->minas = minas;
    partida->estado = PARTIDA_EN_CURSO;
}

static void colocar_minas(Partida *partida, int fila_segura, int columna_segura)
{
    int colocadas = 0;
    int fila;
    int columna;

    while (colocadas < partida->minas) {
        fila = rand() % partida->filas;
        columna = rand() % partida->columnas;

        /* El primer movimiento siempre queda libre de minas. */
        if ((fila == fila_segura && columna == columna_segura) ||
            partida->casillas[fila][columna].mina) {
            continue;
        }

        partida->casillas[fila][columna].mina = 1;
        ++colocadas;
    }

    for (fila = 0; fila < partida->filas; ++fila) {
        for (columna = 0; columna < partida->columnas; ++columna) {
            int desplazamiento_fila;
            int desplazamiento_columna;
            int adyacentes = 0;

            if (partida->casillas[fila][columna].mina) {
                continue;
            }

            for (desplazamiento_fila = -1; desplazamiento_fila <= 1;
                 ++desplazamiento_fila) {
                for (desplazamiento_columna = -1;
                     desplazamiento_columna <= 1; ++desplazamiento_columna) {
                    int vecina_fila = fila + desplazamiento_fila;
                    int vecina_columna = columna + desplazamiento_columna;

                    if (vecina_fila >= 0 && vecina_fila < partida->filas &&
                        vecina_columna >= 0 && vecina_columna < partida->columnas &&
                        partida->casillas[vecina_fila][vecina_columna].mina) {
                        ++adyacentes;
                    }
                }
            }

            partida->casillas[fila][columna].adyacentes = adyacentes;
        }
    }

    partida->minas_colocadas = 1;
}

static void expandir_casillas(Partida *partida, int fila, int columna)
{
    /* Una cola evita recursión al descubrir zonas grandes de casillas vacías. */
    Posicion cola[MAX_FILAS * MAX_COLUMNAS];
    int inicio = 0;
    int fin = 0;
    int desplazamiento_fila;
    int desplazamiento_columna;

    partida->casillas[fila][columna].abierta = 1;
    ++partida->abiertas;
    cola[fin].fila = fila;
    cola[fin].columna = columna;
    ++fin;

    while (inicio < fin) {
        Posicion actual = cola[inicio++];

        if (partida->casillas[actual.fila][actual.columna].adyacentes != 0) {
            continue;
        }

        for (desplazamiento_fila = -1; desplazamiento_fila <= 1;
             ++desplazamiento_fila) {
            for (desplazamiento_columna = -1;
                 desplazamiento_columna <= 1; ++desplazamiento_columna) {
                int vecina_fila = actual.fila + desplazamiento_fila;
                int vecina_columna = actual.columna + desplazamiento_columna;
                Casilla *vecina;

                if (vecina_fila < 0 || vecina_fila >= partida->filas ||
                    vecina_columna < 0 || vecina_columna >= partida->columnas) {
                    continue;
                }

                vecina = &partida->casillas[vecina_fila][vecina_columna];
                if (vecina->abierta || vecina->bandera || vecina->mina) {
                    continue;
                }

                vecina->abierta = 1;
                ++partida->abiertas;
                if (vecina->adyacentes == 0) {
                    cola[fin].fila = vecina_fila;
                    cola[fin].columna = vecina_columna;
                    ++fin;
                }
            }
        }
    }
}

static void abrir_casilla(Partida *partida)
{
    Casilla *seleccionada =
        &partida->casillas[partida->fila_cursor][partida->columna_cursor];

    if (seleccionada->abierta || seleccionada->bandera) {
        return;
    }

    if (!partida->minas_colocadas) {
        colocar_minas(partida, partida->fila_cursor, partida->columna_cursor);
    }

    if (seleccionada->mina) {
        seleccionada->abierta = 1;
        partida->estado = PARTIDA_PERDIDA;
        return;
    }

    expandir_casillas(partida, partida->fila_cursor, partida->columna_cursor);
    if (partida->abiertas == partida->filas * partida->columnas - partida->minas) {
        partida->estado = PARTIDA_GANADA;
    }
}

static int dibujar_partida(const Partida *partida)
{
    int fila;
    int columna;

    if (!limpiar_pantalla()) {
        return 0;
    }

    printf("BUSCAMINAS - Cursor: fila %d, columna %d\n",
           partida->fila_cursor + 1, partida->columna_cursor + 1);
    puts("Flechas: mover | a: abrir | b: poner/quitar bandera");
    putchar('\n');

    printf("    ");
    for (columna = 0; columna < partida->columnas; ++columna) {
        printf("%2d ", columna + 1);
    }
    putchar('\n');

    for (fila = 0; fila < partida->filas; ++fila) {
        printf("%2d |", fila + 1);
        for (columna = 0; columna < partida->columnas; ++columna) {
            const Casilla *casilla = &partida->casillas[fila][columna];
            char simbolo = '#';

            if (partida->estado == PARTIDA_PERDIDA && casilla->mina) {
                simbolo = '*';
            } else if (casilla->abierta ||
                       partida->estado == PARTIDA_GANADA) {
                if (casilla->mina) {
                    simbolo = '*';
                } else if (casilla->adyacentes > 0) {
                    simbolo = (char) ('0' + casilla->adyacentes);
                } else {
                    simbolo = '.';
                }
            } else if (casilla->bandera) {
                simbolo = 'B';
            }

            if (fila == partida->fila_cursor &&
                columna == partida->columna_cursor) {
                printf(">%c ", simbolo);
            } else {
                printf(" %c ", simbolo);
            }
        }
        putchar('\n');
    }

    printf("\nBanderas: ");
    {
        int banderas = 0;
        for (fila = 0; fila < partida->filas; ++fila) {
            for (columna = 0; columna < partida->columnas; ++columna) {
                banderas += partida->casillas[fila][columna].bandera;
            }
        }
        printf("%d / %d\n", banderas, partida->minas);
    }
    if (fflush(stdout) == EOF) {
        return 0;
    }
    return 1;
}

static int jugar(int filas, int columnas, int minas)
{
    Partida partida;
    int tecla;

    inicializar_partida(&partida, filas, columnas, minas);

    while (partida.estado == PARTIDA_EN_CURSO) {
        if (!dibujar_partida(&partida)) {
            fputs("Error al actualizar la pantalla.\n", stderr);
            return 0;
        }
        tecla = leer_tecla();
        if (tecla == TECLA_ERROR) {
            fputs("Error al leer el teclado.\n", stderr);
            return 0;
        }

        switch (tecla) {
        case TECLA_ARRIBA:
            if (partida.fila_cursor > 0) {
                --partida.fila_cursor;
            }
            break;
        case TECLA_ABAJO:
            if (partida.fila_cursor + 1 < partida.filas) {
                ++partida.fila_cursor;
            }
            break;
        case TECLA_IZQUIERDA:
            if (partida.columna_cursor > 0) {
                --partida.columna_cursor;
            }
            break;
        case TECLA_DERECHA:
            if (partida.columna_cursor + 1 < partida.columnas) {
                ++partida.columna_cursor;
            }
            break;
        case 'a':
        case 'A':
        case TECLA_ENTER:
            abrir_casilla(&partida);
            break;
        case 'b':
        case 'B':
            if (!partida.casillas[partida.fila_cursor][partida.columna_cursor].abierta) {
                Casilla *casilla =
                    &partida.casillas[partida.fila_cursor][partida.columna_cursor];
                casilla->bandera = !casilla->bandera;
            }
            break;
        default:
            break;
        }
    }

    if (!dibujar_partida(&partida)) {
        fputs("Error al actualizar la pantalla.\n", stderr);
        return 0;
    }
    if (partida.estado == PARTIDA_GANADA) {
        puts("\nVictoria! Descubriste todas las casillas sin minas.");
    } else {
        puts("\nDerrota! Pisaste una mina. Se muestra la ubicacion de todas las minas.");
    }
    puts("Pulsa cualquier tecla para volver al menu principal.");
    fflush(stdout);
    if (leer_tecla() == TECLA_ERROR) {
        fputs("Error al leer el teclado.\n", stderr);
        return 0;
    }
    return 1;
}

int main(void)
{
    const char *opciones_principales[] = { "Jugar", "Salir" };
    const char *opciones_dificultad[] = {
        "Principiante (8 x 8, 10 minas)",
        "Intermedio (16 x 16, 40 minas)",
        "Experto (16 x 30, 99 minas)"
    };
    int resultado = 0;

    srand((unsigned int) time(NULL));

    if (!iniciar_terminal()) {
        fputs("No se pudo configurar la entrada del terminal.\n", stderr);
        return 1;
    }

    for (;;) {
        int opcion = mostrar_menu("MENU PRINCIPAL", opciones_principales, 2);

        if (opcion < 0) {
            fputs("Error al leer el teclado o actualizar la pantalla.\n", stderr);
            resultado = 1;
            break;
        }
        if (opcion == 1) {
            break;
        }

        opcion = mostrar_menu("SELECCIONA LA DIFICULTAD", opciones_dificultad, 3);
        if (opcion < 0) {
            fputs("Error al leer el teclado o actualizar la pantalla.\n", stderr);
            resultado = 1;
            break;
        }

        switch (opcion) {
        case 0:
            if (!jugar(8, 8, 10)) {
                resultado = 1;
            }
            break;
        case 1:
            if (!jugar(16, 16, 40)) {
                resultado = 1;
            }
            break;
        case 2:
            if (!jugar(16, 30, 99)) {
                resultado = 1;
            }
            break;
        default: break;
        }
        if (resultado != 0) {
            break;
        }
    }

    if (!restaurar_terminal()) {
        fputs("No se pudo restaurar el modo original del terminal.\n", stderr);
        resultado = 1;
    }

    return resultado;
}