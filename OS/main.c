/* SPDX-License-Identifier: GPL-2.0-Only
* Copyright (C) 2026 MDFJ
* Copyright (c) mauriminuano125-a11y 2026
* Copyright (c) Fire Mint - Organization official 2026 */

/* main.c, formerly created by MDFJ, is now giving us errors. It was created by the Fire Mint community, specifically by mauriminuano125-a11y.*/
/* Repository: https://github.com/mauriminuano125-a11y/firemint.git */


#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <stdlib.h>

/***
CarlOS V1.0.1 LTS - Versión Oficial
 * Sistema operativo ligero diseñado para bajo consumo de RAM.
 * Compatible con hardware antiguo y moderno.
 * Soporte integrado para juegos de Windows vía Proton.
 */

typedef enum {
    VERSION_NONE,
    VERSION_SHELL,
    VERSION_BUSINESS,
    VERSION_PERSONAL
} OSVersion;

typedef struct {
    char username[50];
    char password[50];
    OSVersion version;
    bool is_setup;
} SystemConfig;

SystemConfig config = {"", "", VERSION_NONE, false};

void clear_screen() {
    printf("\033[H\033[2J\033[3J");
    fflush(stdout);
}

void save_config() {
    FILE *f = fopen("config.bin", "wb");
    if (f) {
        fwrite(&config, sizeof(SystemConfig), 1, f);
        fclose(f);
    }
}

void load_config() {
    FILE *f = fopen("config.bin", "rb");
    if (f) {
        if (fread(&config, sizeof(SystemConfig), 1, f) != 1) {
            config.is_setup = false;
        }
        fclose(f);
    } else {
        config.is_setup = false;
    }
}

void setup_system() {
    char input[100];
    clear_screen();
    printf("\r\n\033[1;37m====================================\033[0m\r\n");
    printf("\033[1;37m       CarlOS v1.0.1 LTS           \033[0m\r\n");
    printf("\033[1;37m====================================\033[0m\r\n\r\n");
    printf("\033[1;37mWelcome to Carl-OS v1.0.1 LTS!\033[0m\r\n");
    
    printf("\033[1;37mSelect your edition:\033[0m\r\n");
    printf("\033[1;37m1. Shell (Advanced Use)\033[0m\r\n");
    printf("\033[1;37m2. Personal (Games and Multimedia)\033[0m\r\n");
    printf("\r\n\033[1;37mOption (1-2): \033[0m");
    fflush(stdout);
    
    if (fgets(input, sizeof(input), stdin)) {
        int choice = atoi(input);
        switch(choice) {
            case 1: config.version = VERSION_SHELL; break;
            case 2: config.version = VERSION_PERSONAL; break;
            default: config.version = VERSION_PERSONAL; break;
        }
    }

    printf("\r\n\033[1;37mUsername: \033[0m");
    fflush(stdout);
    if (fgets(config.username, sizeof(config.username), stdin)) {
        config.username[strcspn(config.username, "\n")] = 0;
        config.username[strcspn(config.username, "\r")] = 0;
    }

    printf("\033[1;37mPassword: \033[0m");
    fflush(stdout);
    if (fgets(config.password, sizeof(config.password), stdin)) {
        config.password[strcspn(config.password, "\n")] = 0;
        config.password[strcspn(config.password, "\r")] = 0;
    }

    config.is_setup = true;
    save_config();
    printf("\r\n\033[1;32mConfiguration completed successfully!\033[0m\r\n");
    printf("\033[1;37mPress Enter to start Fire Mint...\033[0m\r\n");
    fflush(stdout);
    
    char dummy[10];
    if (fgets(dummy, sizeof(dummy), stdin)) {}
}

bool login() {
    char pass[50];
    char option[10];
    clear_screen();
    printf("\r\n\033[1;37m------------------------------------\033[0m\r\n");
    printf("\033[1;37m    CARL-OS \033[0m\r\n");
    printf("\033[1;37m------------------------------------\033[0m\r\n\r\n");
    printf("\033[1;37mUsername: %s\033[0m\r\n", config.username);
    printf("\033[1;37mPassword: \033[0m");
    fflush(stdout);
    if (fgets(pass, sizeof(pass), stdin)) {
        pass[strcspn(pass, "\n")] = 0;
        pass[strcspn(pass, "\r")] = 0;
        if (strcmp(pass, config.password) == 0) return true;
    }
    printf("\r\n\033[1;31mInvalid password.\033[0m\r\n");
    printf("\033[1;37mPress 2 to reset the password.\033[0m\r\n");
    printf("\033[1;37mPress Enter to retry.\033[0m\r\n");
    fflush(stdout);
    if (fgets(option, sizeof(option), stdin)) {
        option[strcspn(option, "\n")] = 0;
        option[strcspn(option, "\r")] = 0;
        if (strcmp(option, "2") == 0) {
            printf("\r\n\033[1;32mPassword reset\033[0m\r\n");
            printf("\033[1;37mNew password: \033[0m");
            fflush(stdout);
            char new_pass[50];
            if (fgets(new_pass, sizeof(new_pass), stdin)) {
                new_pass[strcspn(new_pass, "\n")] = 0;
                new_pass[strcspn(new_pass, "\r")] = 0;
                strcpy(config.password, new_pass);
                save_config();
                printf("\r\n\033[1;32mPassword updated successfully.\033[0m\r\n");
                printf("\033[1;37mPress Enter to continue.\033[0m\r\n");
                fflush(stdout);
                char dummy[10];
                if (fgets(dummy, sizeof(dummy), stdin)) {}
                return true;
            }
        }
    }
    
    return false;
}

void pc_settings() {
    char input[100];
    while (true) {
        clear_screen();
        printf("\r\n\033[1;37m=== PC CONFIGURATION ===\033[0m\r\n");
        printf("1. System -> General system settings\r\n");
        printf("2. Applications -> Install or manage packages\r\n");
        printf("3. Battery -> RAM and power optimization\r\n");
        printf("4. Hardware -> View CPU, RAM, and disks\r\n");
        printf("5. Network -> Configure servers and connection\r\n");
        printf("6. Exit -> Return to terminal\r\n");
        printf("\r\nSelection (1-6): ");
        fflush(stdout);
        
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        int choice = atoi(input);
        
        if (choice == 1) {
            printf("\r\n\033[1;37mSistema: Fire Mint (Oficial)\r\nEstado: Compatible con hardware viejo/nuevo.\r\nOptimizacion: Bajo consumo de RAM.\033[0m\r\n");
        } else if (choice == 2) {
            char app_cmd[100];
            printf("\r\n\033[1;37mGestor de Aplicaciones:\033[0m\r\n");
            printf("Uso: pc install [lenguaje] o pc principal install [archivo]\r\n");
            printf("PC Application > ");
            fflush(stdout);
            if (fgets(app_cmd, sizeof(app_cmd), stdin)) {
                app_cmd[strcspn(app_cmd, "\n")] = 0;
                app_cmd[strcspn(app_cmd, "\r")] = 0;
                if (strncmp(app_cmd, "pc install ", 11) == 0) {
                    printf("\r\n\033[1;32mInstalando lenguaje: %s...\033[0m\r\n", app_cmd + 11);
                } else if (strncmp(app_cmd, "pc principal install ", 21) == 0) {
                    printf("\r\n\033[1;32mInstalando archivo principal: %s...\033[0m\r\n", app_cmd + 21);
                } else if (strlen(app_cmd) > 0) {
                    printf("\r\n\033[1;31mComando de aplicacion desconocido.\033[0m\r\n");
                }
            }
        } else if (choice == 3) {
            printf("\r\n\033[1;37mBateria y Optimizacion:\r\nRAM: Modo ahorro extremo activado.\r\nConsumo optimizado para PCs antiguas.\033[0m\r\n");
        } else if (choice == 4) {
            printf("\r\n\033[1;37mInformacion de Hardware:\r\nRAM: 8GB (Consumo reducido)\r\nCPU: Soporte Multi-core Universal.\r\nDisco: Carl SSD v10.01 detectado.\033[0m\r\n");
        } else if (choice == 5) {
            printf("\r\n\033[1;37mRed:\r\nConfiguracion: Servidores Carl Cloud activos.\r\nConexion: Estable.\033[0m\r\n");
        } else if (choice == 6) {
            break;
        }
        if (choice >= 1 && choice <= 5) {
            printf("\r\nPresiona Enter para volver...");
            fflush(stdout);
            char wait[10];
            if (fgets(wait, sizeof(wait), stdin)) {}
        }
    }
}

int main() {
    load_config();
    if (!config.is_setup) setup_system();
    while (!login());

    char input[100];
    clear_screen();
    printf("\033[1;32m¡Bienvenido a Carl OS v10.01 LTS Oficial, %s!\033[0m\r\n", config.username);
    printf("\033[1;37mEl sistema mas ligero y compatible con Proton.\033[0m\r\n");
    printf("\033[1;37mEscribe 'help' para ver los comandos.\033[0m\r\n");
    fflush(stdout);

    while (true) {
        printf("\r\n\033[1;37mFire Mint> \033[0m");
        fflush(stdout);
        if (fgets(input, sizeof(input), stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;
        input[strcspn(input, "\r")] = 0;

        if (strcmp(input, "exit") == 0) {
            printf("\r\nApagando Carl OS v10.01 LTS...\r\n");
            break;
        } else if (strcmp(input, "help") == 0) {
            printf("\r\n\033[1;37mComandos de Carl OS v10.01 LTS Oficial:\033[0m\r\n");
            printf(" moon install [paquete]       -> Instala un paquete o juego (Proton)\r\n");
            printf(" moon update on [paquete]     -> Activa actualizaciones automaticas\r\n");
            printf(" moon update off [paquete]    -> Desactiva actualizaciones automaticas\r\n");
            printf(" actualict moon [paquete]     -> Actualiza manualmente el paquete\r\n");
            printf(" moon open [paquete]          -> Abrir aplicacion via web\r\n");
            printf(" pc settings                  -> Abre el menu de configuracion\r\n");
            printf(" apt -v                       -> Muestra la version del sistema\r\n");
            printf(" carl -ver                    -> Muestra la version del terminal\r\n");
            printf(" apt shell update install     -> Actualiza el terminal (1 minuto)\r\n");
            printf(" hardware pc ram              -> Muestra la cantidad de RAM\r\n");
            printf(" hardware pc cpu              -> Muestra los nucleos de la CPU\r\n");
            printf(" fire enter desktop           -> Abre el escritorio con Firefox\r\n");
            printf(" antivirus scan [ruta]        -> Escanea un archivo o carpeta\r\n");
            printf(" antivirus scan full          -> Escaneo completo del sistema\r\n");
            printf(" antivirus update             -> Actualiza las firmas del antivirus\r\n");
            printf(" antivirus status             -> Muestra el estado del antivirus\r\n");
            printf(" antivirus quarantine         -> Ver archivos en cuarentena\r\n");
            printf(" antivirus -v                 -> Version del antivirus\r\n");
            printf(" logout                       -> Cerrar sesion\r\n");
        } else if (strcmp(input, "fire enter desktop") == 0) {
            printf("\r\n\033[1;32mIniciando Fire Mint Desktop...\033[0m\r\n");
            printf("Cargando wallpaper...\r\n");
            printf("Iniciando Firefox...\r\n");
            printf("Configurando User-Agent: FireMint v01.10.01 LTS...\r\n");
            printf("\033[1;33mInterfaz grafica activa en puerto 8080\033[0m\r\n");
        } else if (strcmp(input, "pc settings") == 0) {
            pc_settings();
        } else if (strncmp(input, "moon install ", 13) == 0) {
            printf("\r\n\033[1;32mInstalando %s usando Proton...\033[0m\r\n", input + 13);
            printf("Configurando compatibilidad con Windows...\r\nListo.\r\n");
        } else if (strncmp(input, "moon update on ", 15) == 0) {
            printf("\r\n\033[1;37mActualizaciones auto ACTIVADAS para: %s\033[0m\r\n", input + 15);
        } else if (strncmp(input, "moon update off ", 16) == 0) {
            printf("\r\n\033[1;37mActualizaciones auto DESACTIVADAS para: %s\033[0m\r\n", input + 16);
        } else if (strncmp(input, "actualict moon ", 15) == 0) {
            printf("\r\n\033[1;37mActualizando manualmente: %s...\033[0m\r\n", input + 15);
        } else if (strncmp(input, "moon open ", 10) == 0) {
            printf("\r\n\033[1;32mAbriendo %s via Web...\033[0m\r\n", input + 10);
        } else if (strcmp(input, "apt -v") == 0) {
            printf("\r\n\033[1;37mCarl OS Version: 10.01 LTS (Oficial)\033[0m\r\n");
        } else if (strcmp(input, "carl -ver") == 0) {
            printf("\r\n\033[1;37mTerminal Version: 2.5.0-LTS\033[0m\r\n");
        } else if (strcmp(input, "apt shell update install") == 0) {
            printf("\r\n\033[1;33mIniciando actualizacion de shell (1 minuto)...\033[0m\r\n");
            printf("Descargando componentes oficiales...\r\nInstalacion completa.\r\n");
        } else if (strcmp(input, "hardware pc ram") == 0) {
            printf("\r\n\033[1;37mRAM: 8 GB detectados. Consumo ultra bajo habilitado.\033[0m\r\n");
        } else if (strcmp(input, "hardware pc cpu") == 0) {
            printf("\r\n\033[1;37mCPU: 8 Nucleos detectados. Compatible con procesadores modernos y antiguos.\033[0m\r\n");
        } else if (strcmp(input, "antivirus scan full") == 0) {
            printf("\r\n\033[1;33m╔══════════════════════════════════════╗\033[0m\r\n");
            printf("\033[1;33m║   FIRE-M.I.N.T   ║\033[0m\r\n");
            printf("\033[1;33m╚══════════════════════════════════════╝\033[0m\r\n\r\n");
            printf("  Cargando motor de escaneo...\r\n");
            printf("  Cargando base de firmas v1.0.0-LTS...\r\n");
            printf("  Iniciando analisis heuristico...\r\n\r\n");
            printf("  Escaneando carl/user/run/home/user ...      \033[1;32m[OK]\033[0m\r\n");
            printf("  Escaneando carl/user/run/system/user ...    \033[1;32m[OK]\033[0m\r\n");
            printf("  Escaneando carl/user/run/bin/user ...       \033[1;32m[OK]\033[0m\r\n");
            printf("  Escaneando carl/user/run/temp/user ...      \033[1;32m[OK]\033[0m\r\n");
            printf("  Escaneando carl/user/run/appdata/user ...   \033[1;32m[OK]\033[0m\r\n\r\n");
            printf("  \033[1;37mProgreso: [####################] 100%%\033[0m\r\n\r\n");
            printf("  \033[1;37mArchivos escaneados: \033[0m1,284\r\n");
            printf("  \033[1;32mAmenazas detectadas: \033[0m0\r\n");
            printf("  \033[1;32mArchivos en cuarentena: \033[0m0\r\n\r\n");
            printf("\033[1;32m[✓] Sistema limpio. Fire Mint esta protegido.\033[0m\r\n");
        } else if (strncmp(input, "antivirus scan ", 15) == 0) {
            const char *target = input + 15;
            printf("\r\n\033[1;33m[Carl Antivirus] Escaneando: carl/user/run/%s/user\033[0m\r\n", target);
            printf("  Cargando firmas de malware v1.0.0-LTS...\r\n");
            printf("  Analizando firma MD5/SHA256...\r\n");
            printf("  Analizando comportamiento heuristico...\r\n");
            printf("  Verificando patrones de red...\r\n");
            printf("  Verificando acceso al sistema de archivos...\r\n\r\n");
            printf("  \033[1;37mNivel de amenaza: \033[1;32mLIMPIO\033[0m\r\n");
            printf("  \033[1;37mDeteccion: \033[0mNinguna\r\n\r\n");
            printf("\033[1;32m[✓] Escaneo completado. carl/user/run/%s/user esta limpio.\033[0m\r\n", target);
        } else if (strcmp(input, "antivirus update") == 0) {
            printf("\r\n\033[1;33m[Carl Antivirus] Actualizando base de datos...\033[0m\r\n");
            printf("  Conectando a servidores Carl Cloud...\r\n");
            printf("  Descargando nuevas firmas...\r\n");
            printf("  Actualizando motor heuristico...\r\n");
            printf("  Verificando integridad de la base de datos...\r\n\r\n");
            printf("\033[1;32m[✓] Actualizado correctamente.\033[0m\r\n");
            printf("  Firmas:   Version 1.0.0-LTS\r\n");
            printf("  Motor:    Carl AV Engine 1.0\r\n");
            printf("  Fuente:   Carl Cloud Official\r\n");
        } else if (strcmp(input, "antivirus quarantine") == 0) {
            printf("\r\n\033[1;37m=== Carl Antivirus - Cuarentena ===\033[0m\r\n");
            printf("  Archivos en cuarentena: \033[1;32m0\033[0m\r\n\r\n");
            printf("  \033[0;37mNo hay archivos en cuarentena.\033[0m\r\n");
            printf("  El sistema esta completamente limpio.\r\n");
        } else if (strcmp(input, "antivirus -v") == 0) {
            printf("\r\n\033[1;33m╔══════════════════════════════════════╗\033[0m\r\n");
            printf("\033[1;33m║         FIRE-M.I.N.T       ║\033[0m\r\n");
            printf("\033[1;33m╚══════════════════════════════════════╝\033[0m\r\n\r\n");
            printf("  Distribucion: \033[1;31mFire Mint\033[0m\r\n");
            printf("  Basado en:    Carl OS Official\r\n");
            printf("  Motor:        Carl AV Engine 1.0\r\n");
            printf("  Firmas:       v1.0.0-LTS\r\n");
            printf("  Heuristica:   Habilitada\r\n");
            printf("  Licencia:     \033[1;37mGPLv2\033[0m\r\n");
            printf("  Autor:        bentosmau-gif\r\n");
        } else if (strcmp(input, "antivirus status") == 0) {
            printf("\r\n\033[1;33m╔══════════════════════════════════════╗\033[0m\r\n");
            printf("\033[1;33m║      FIRE-M.I.N.T Status.     ║\033[0m\r\n");
            printf("\033[1;33m╚══════════════════════════════════════╝\033[0m\r\n\r\n");
            printf("  Estado:           \033[1;32mACTIVO\033[0m\r\n");
            printf("  Proteccion:       \033[1;32mTiempo real ON\033[0m\r\n");
            printf("  Motor:            Carl AV Engine 1.0\r\n");
            printf("  Firmas:           v1.0.0-LTS\r\n");
            printf("  Heuristica:       \033[1;32mHabilitada\033[0m\r\n");
            printf("  Cuarentena:       0 archivos\r\n");
            printf("  Ultimo escaneo:   Al iniciar sesion\r\n");
            printf("  Licencia:         GPLv2 - Carl OS Official\r\n");
        } else if (strcmp(input, "logout") == 0) {
            while (!login());
            clear_screen();
            printf("\033[1;32m¡Sesion reiniciada con exito!\033[0m\r\n");
        } else if (strlen(input) > 0) {
            printf("\r\n\033[1;31mComando desconocido.\033[0m\r\n");
        }
        fflush(stdout);
    }
    return 0;
}
