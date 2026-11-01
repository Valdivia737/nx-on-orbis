# NX on Orbis — mejoras para PS4 Pro (rama `mejoras-ps4-pro`)

Fork de [alechurri/nx-on-orbis](https://github.com/alechurri/nx-on-orbis) (port experimental del emulador
de Switch **Eden** a PS4 Pro) mantenido por **Valdivia737**. Esta rama recoge el trabajo de una semana
de pruebas en una **PS4 Pro con firmware 10.50 y GoldHEN**: de un Eden que con Super Mario Bros. Wonder
bajaba a 1 FPS y se congelaba a los 2–3 minutos, a uno que **juega Wonder 30 minutos seguidos a 26–30 FPS**.

> Escrito por Claude (asistente de IA de Anthropic) a partir de las sesiones de trabajo con Valdivia737.
> Todo lo que aparece como resultado está **medido** en la consola con los registros que hay en
> [`registros/`](registros/). Lo que no está comprobado se marca como **HIPÓTESIS**.

**Sigue siendo experimental.** No incluye juegos, claves ni firmware de Switch: cada uno pone los suyos.
Licencia GPL-3.0, igual que el proyecto original y Eden.

---

## Resultados (PS4 Pro, fw 10.50)

| Juego | Formato | Estado | FPS jugando | Versión | De dónde sale el dato |
|---|---|---|---|---|---|
| Super Mario Bros. Wonder | XCI | **Estable**: 32 min seguidos sin fallos y guarda partida | Niveles **26–30**, mapa del mundo **44–56** | jitcache10–11 | Medido en vivo con jitcache10 (ese registro no se guardó). Con jitcache8: [`jitcache8_20261031_mundo_ok`](registros/eden-propio/jitcache8_20261031_mundo_ok/) (guarda los primeros 12 min) |
| Super Mario 3D World + Bowser's Fury | XCI | **Estable**: 15,8 min sin fallos, memoria quieta | Niveles **23–30** (mediana 27), menús 54–60 | jitcache11 | [`jitcache11_20261101_3dworld`](registros/eden-propio/jitcache11_20261101_3dworld/) |
| Paper Mario: La Puerta Milenaria | NSP | **Jugable pero lento**, en español: 16,9 min sin fallos | **7–9** (mediana 8,9), menús hasta 26. Una cinemática bajó a 0,2–0,5 | jitcache11 | [`jitcache11_papermario_boot.log`](registros/eden-propio/jitcache11_papermario_boot.log) |
| Metroid Dread | NSP | **No carga**: al cargar pide una reserva de 1,8 GiB y falla | — | — | Pendiente |

### Registro de FPS por sesión (sacado automáticamente de `boot.log`)

| Sesión (`registros/eden-propio/`) | Versión | Juego | Minutos | FPS mediana | Peor 10 % | Máx. | Qué pasó |
|---|---|---|---|---|---|---|---|
| `jitcache1_20261031_sesion1` | jitcache1 | Wonder | 6,0 | 4,3 | 1,0 | 44,5 | Bajones a 1 FPS: la caché del JIT se vació 25/28/10 veces |
| `jitcache1_20261031_sesion2_garlic0` | jitcache1 | Wonder | 7,3 | 6,2 | 1,0 | 41,5 | Lo mismo: 31/34/13 vaciados |
| `jitcache1_20261031_sesion3_garlic0_cache128` | jitcache1 | Wonder | 5,9 | 30,0 | 23,5 | 38,1 | Sin bajones (caché de 128 MiB), pero se quedó **sin memoria** a los 4 min |
| `jitcache2_diag_boot.log` | jitcache2 | Wonder | 3,4 | 30,0 | 10,9 | 33,5 | Sin memoria (`bad_alloc`): así se descubrió el problema del heap |
| `jitcache4_20261031_cache64` | jitcache4 | Wonder | 4,5 | 18,9 | 10,8 | 54,1 | Ningún fallo: heap arreglado |
| `jitcache6_20261031_arena1792_vma` | jitcache6 | Wonder | 6,5 | 21,2 | 6,7 | 50,9 | Congelado a los 213 s con arena 1792 (configuración descartada) |
| `jitcache8_20261031_mundo_ok` | jitcache8 | Wonder | 12,0 | 22,5 | 8,9 | 57,4 | Ningún fallo: el mapa del mundo carga |
| `jitcache11_papermario_boot.log` | jitcache11 | Paper Mario | 16,9 | 8,9 | 7,0 | 26,5 | Ningún fallo; lo limita la CPU |
| `jitcache11_20261101_3dworld` | jitcache11 | 3D World | 15,8 | 27,0 | 23,2 | 60,0 | Ningún fallo; un solo vaciado del JIT |

- La mediana y el "peor 10 %" se calculan con todas las líneas `status:` de 1 FPS o más, así que incluyen cargas y menús.
- El "sin imagen durante 15 s" que sale en todas las sesiones hacia los 16 s es la carga inicial, y es normal.

**El límite hoy es la CPU, no la GPU.** En los tres juegos que funcionan, el núcleo que ejecuta el código
principal del juego va al 93–97 % y la GPU espera el 0 % del tiempo. Bajar la resolución no ayuda; lo que
ayudaría es abaratar el código que genera el JIT (ver Pendientes).

---

## Qué estaba mal y cómo se arregló (en orden)

Cada paso se encontró midiendo con el registro de la consola (`boot.log`), no a ojo.

| # | Problema medido | Arreglo | Versión |
|---|---|---|---|
| 1 | Bajones periódicos a ~1 FPS: la caché de código del JIT (32 MiB por núcleo) se llenaba y se vaciaba entera, y el juego se recompilaba durante un minuto | Caché ajustable desde `settings.txt` (`NX_JIT_CACHE_MIB`), contador de vaciados en el registro (parche 0028) | jitcache1 |
| 2 | Al subir la caché, congelamientos por falta de memoria: el heap de musl crecía a saltos exponenciales (128, 256, 512 MiB) aunque se pidieran 96 bytes | `__expand_heap` propio con un paso máximo de 32 MiB (`frontend/libc_heap_fixes.cpp`) | jitcache4 |
| 3 | La tabla de páginas de la GPU reservaba 128 MiB de golpe | Tabla perezosa (parche 0029) | jitcache3 |
| 4 | Al entrar a un nivel, la ventana de memoria de la GPU se llenaba: las texturas ASTC se descomprimían a RGBA8 (4 bytes por píxel) | Opción `astc_recompress=bc3` (1 byte por píxel) y estadísticas de memoria de la GPU en el registro (parches 0030 y 0031) | jitcache5–7 |
| 5 | El mapa del mundo de Wonder no cargaba por el reparto de memoria | Búfer de vídeo y cola de descriptores ajustables (parche 0032) | jitcache8 |
| 6 | Robustez ante falta de memoria de la GPU | Liberar memoria y reintentar cuando falla una reserva, **adaptado de [Eden4](https://github.com/xatyy/Eden4) de xatyy** (parches 0033 y 0034) | jitcache9–10 |
| 7 | Los juegos salían en inglés | `language=` y `region=` en `settings.txt` | jitcache11 |
| 8 | **Rojo y azul cambiados en toda la imagen** (problema que el autor original dejó abierto) | La imagen final se dibujaba en orden BGRA sobre una pantalla RGBA; ahora se usa el orden de la pantalla, como ya se hacía en Android (parche 0035) | jitcache12 (**colores pendientes de confirmar en la consola**) |

Los cambios de Eden están en [`patches/eden/`](patches/eden/) (0028 a 0035) y los del frontend en
[`frontend/`](frontend/). Todo va dentro de `#ifdef __ORBIS__`: no afecta a otras plataformas.

---

## `settings.txt` recomendado

Va en `/data/edenps4/settings.txt` de la PS4:

```
env=ORBIS_VRAM_GARLIC=0
env=ORBIS_ARENA_MIB=1408
env=NX_JIT_CACHE_MIB=64
env=NX_JIT_CACHE_MIB_CORE3=8
env=NX_TEX_CACHE_PCT=40
env=NX_BUF_CACHE_PCT=20
astc_recompress=bc3
env=NX_STREAM_BUFFER_MIB=128
env=NX_DESC_PAYLOAD=0x20000
language=es-419
region=usa
```

- `language=es-419` es español latino; para español de España: `language=es` y `region=europe`.
- Combinaciones que **fallaron**: caché del JIT a 64/128 sin el arreglo del heap (se congela); `ORBIS_ARENA_MIB=1792` (deja sin memoria al resto); `NX_RECLAIM_LOW_MIB` (el presupuesto que da el driver siempre es 0).
- Solo jitcache12: `env=NX_PRESENT_ORDER=auto|swap|off` para probar el arreglo de colores (`auto` por defecto, `off` = como antes).

---

## Instalar

1. Descarga el `.pkg` de la versión que quieras en [Releases](../../releases). El más nuevo es jitcache12; el último probado a fondo es jitcache11.
2. Instálalo con tu instalador de PKG habitual. Se instala como `EDPS00001` y reemplaza al Eden anterior.
3. Pon en la PS4 tus claves (`/data/edenps4/keys`), tu firmware (`/data/edenps4/firmware`, archivos `.nca`) y tus juegos (`/data/edenps4/roms`), igual que con el original.
4. Copia el `settings.txt` de arriba.

---

## Leer los registros

En la PS4 (por FTP): `/data/edenps4/boot.log`, `mesa.log` y `log/eden_log.txt`.
- La primera línea de `boot.log` dice qué versión corre.
- Cada 10 s: `status: game X fps … direct free … jit cache flushes a/b/c/d` (FPS, memoria libre y vaciados de la caché del JIT).
- Cada 30 s: `profile [CPUCore_0]: busy X% (jit Y%)`: si un núcleo va al ~95 % con jit alto, el límite es la CPU.
- `mesa.log` → `BUDGET … waited N ms for the GPU`: si es ~0 %, la GPU no es el cuello de botella.

[`registros/`](registros/) guarda las sesiones reales de cada versión:
- `eden-propio/`: nuestro Eden (Wonder, Paper Mario y 3D World).
- `eden4-xatyy/`: la prueba de Eden4 1.0. Arranca en fw 10.50, pero Wonder se aborta dentro del juego (`nn::oe::EnableGamePlayRecording`).

---

## Pendientes

1. Confirmar en la consola el arreglo de colores (jitcache12).
2. **Más FPS en la CPU**, de lo más barato a lo más caro:
   - modo de CPU "Unsafe" (NaN y coma flotante menos exactos);
   - portar las optimizaciones del JIT de Eden4 (recorrido de la tabla de páginas en línea, reutilizar la búsqueda de página, R13 asignable);
   - fastmem.
   HIPÓTESIS: +10–30 % entre todas.
3. Paper Mario: cinemática a 0,2–0,5 FPS con un núcleo en espera activa.
4. Metroid Dread: averiguar quién pide 1,8 GiB al cargar.
5. Fallo intermitente al arrancar (`radv_create_shader_arena`); reabrir la app lo soluciona.

## Créditos

- **alechurri**: NX on Orbis, el port original y todas sus herramientas.
- **Eden Emulator Project** / yuzu: el emulador.
- **xatyy**: [Eden4](https://github.com/xatyy/Eden4), del que se adaptó el reintento de memoria de la GPU.
- **Valdivia737**: pruebas en la consola, decisiones y este fork.
- **Claude** (Anthropic): análisis de registros, parches y esta documentación.
