// ============================================================================
//  Input Assist - mod para Geode SDK (Geometry Dash 2.2)
//
//  F6 cambia entre modos:
//    0 = NORMAL / OFF               -> juego 100% legitimo
//    1 = Jugador Experto            -> buffer de 2 ticks + coyote de 1 tick
//    2 = Hacker / Auto Frame-Perfect -> buffer amplio + coyote amplio
//
//  Funcionamiento:
//   - PlayerObject::pushButton: si pulsas salto y el jugador aun no puede
//     saltar (esta en el aire), el clic se guarda en un buffer. Si acabas de
//     dejar el suelo (coyote), el salto se concede en ese mismo instante.
//   - PlayerObject::update: cada tick revisa el buffer y ejecuta el clic en el
//     primer tick en que el jugador toca suelo (el frame exacto).
//   - Se muestra el estado en un overlay dentro del juego y, en Windows, en una
//     ventanita de consola independiente.
//
//  Notas:
//   - Un "tick" es cada llamada a PlayerObject::update (physics step).
//   - Solo asiste saltos que dependen del suelo (cubo/bola/robot/spider).
//     Nave, UFO, wave y swing no se tocan. Los orbes los gestiona el juego.
//   - Si tu version de Geode/GD tiene otra firma para
//     CCKeyboardDispatcher::dispatchKeyboardMSG, ajustala (ver abajo).
// ============================================================================

#include <Geode/Geode.hpp>
#include <Geode/modify/PlayerObject.hpp>
#include <Geode/modify/PlayLayer.hpp>
#include <Geode/modify/CCKeyboardDispatcher.hpp>

#ifdef GEODE_IS_WINDOWS
#include <Windows.h>
#endif

#include <string>

using namespace geode::prelude;

// ----------------------------------------------------------------------------
//  Configuracion de modos
// ----------------------------------------------------------------------------
struct ModeInfo {
    const char* title;      // nombre del modo
    const char* status;     // estado corto
    const char* tolerance;  // texto de tolerancia
    int bufferTicks;        // ticks que se recuerda un clic adelantado
    int coyoteTicks;        // ticks de gracia tras dejar el suelo
};

static constexpr ModeInfo MODES[3] = {
    { "Modo 0 - Desactivado",
      "NORMAL / OFF",
      "Sin ayuda (juego 100% legitimo)",
      0, 0 },
    { "Modo 1 - Jugador Experto",
      "ASISTENCIA SUAVE",
      "1-2 ticks de tolerancia",
      2, 1 },
    { "Modo 2 - Hacker / Auto Frame-Perfect",
      "ASISTENCIA MAXIMA",
      "Buffer amplio: clic en el frame exacto",
      12, 4 },
};

static int g_mode = 0;      // modo actual
static int g_assists = 0;   // cuantas veces ha ayudado el mod
static std::string g_lastEvent = "-";

static std::string buildStatusText(bool forConsole) {
    auto const& cur = MODES[g_mode];
    auto const& next = MODES[(g_mode + 1) % 3];

    std::string s;
    s += "== INPUT ASSIST ==\n";
    s += "Estado   : " + std::string(cur.status) + "\n";
    s += "Nivel    : " + std::string(cur.title) + "\n";
    s += "Tolerancia: " + std::string(cur.tolerance) + "\n";
    s += "Asistencias: " + std::to_string(g_assists) + "\n";
    s += "Ultimo   : " + g_lastEvent + "\n";
    s += "F6 -> " + std::string(next.title) + "\n";
    if (forConsole) {
        s += "\n(Esta ventana se actualiza en tiempo real)\n";
    }
    return s;
}

// ----------------------------------------------------------------------------
//  Consola independiente (solo Windows)
// ----------------------------------------------------------------------------
#ifdef GEODE_IS_WINDOWS
static HANDLE g_console = nullptr;

static void openConsoleWindow() {
    // Si el proceso ya tiene una consola (p.ej. la de Geode), AllocConsole falla
    // y simplemente nos quedamos con el overlay dentro del juego.
    if (!AllocConsole()) return;

    g_console = GetStdHandle(STD_OUTPUT_HANDLE);
    SetConsoleTitleA("Input Assist - Estado");
    SetConsoleOutputCP(CP_UTF8);

    DWORD mode = 0;
    if (GetConsoleMode(g_console, &mode)) {
        SetConsoleMode(g_console, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }

    if (HWND hwnd = GetConsoleWindow()) {
        // Ventana pequeña
        MoveWindow(hwnd, 40, 40, 460, 230, TRUE);
        // Quitar el boton de cerrar: cerrar la consola mataria el juego
        if (HMENU menu = GetSystemMenu(hwnd, FALSE)) {
            DeleteMenu(menu, SC_CLOSE, MF_BYCOMMAND);
        }
    }
}

static void writeConsole(std::string const& text) {
    if (!g_console) return;
    std::string out = "\x1b[2J\x1b[H" + text; // limpiar + cursor al inicio
    DWORD written = 0;
    WriteConsoleA(g_console, out.c_str(), static_cast<DWORD>(out.size()), &written, nullptr);
}
#endif

// ----------------------------------------------------------------------------
//  Overlay dentro del juego
// ----------------------------------------------------------------------------
static constexpr const char* OVERLAY_ID = "input-assist-overlay"_spr;

static void refreshUI() {
#ifdef GEODE_IS_WINDOWS
    writeConsole(buildStatusText(true));
#endif
    if (auto pl = PlayLayer::get()) {
        if (auto overlay = pl->m_uiLayer->getChildByID(OVERLAY_ID)) {
            if (auto label = typeinfo_cast<CCLabelBMFont*>(overlay->getChildByID("label"))) {
                label->setString(buildStatusText(false).c_str());
            }
        }
    }
}

// ----------------------------------------------------------------------------
//  Tecla F6
// ----------------------------------------------------------------------------
class $modify(AssistKeyboard, CCKeyboardDispatcher) {
    // Firma para GD 2.2074 con Geode v4+. Si tu version no tiene el parametro
    // 'timestamp', quitalo (tanto aqui como en la llamada a la original).
    bool dispatchKeyboardMSG(enumKeyCodes key, bool down, bool isRepeat, double timestamp) {
        if (down && !isRepeat && key == KEY_F6) {
            g_mode = (g_mode + 1) % 3;
            g_lastEvent = std::string("Cambiado a ") + MODES[g_mode].status;
            log::info("Input Assist: {}", MODES[g_mode].title);
            refreshUI();
        }
        return CCKeyboardDispatcher::dispatchKeyboardMSG(key, down, isRepeat, timestamp);
    }
};

// ----------------------------------------------------------------------------
//  Overlay: se crea al iniciar el nivel
// ----------------------------------------------------------------------------
class $modify(AssistPlayLayer, PlayLayer) {
    bool init(GJGameLevel* level, bool useReplay, bool dontCreateObjects) {
        if (!PlayLayer::init(level, useReplay, dontCreateObjects)) return false;

        auto overlay = CCNode::create();
        overlay->setID(OVERLAY_ID);

        auto bg = CCLayerColor::create({ 0, 0, 0, 170 }, 250.f, 105.f);
        overlay->addChild(bg);

        auto label = CCLabelBMFont::create(buildStatusText(false).c_str(), "chatFont.fnt");
        label->setID("label");
        label->setAnchorPoint({ 0.f, 1.f });
        label->setAlignment(kCCTextAlignmentLeft);
        label->setScale(0.55f);
        label->setPosition({ 6.f, 100.f });
        overlay->addChild(label);

        auto winSize = CCDirector::get()->getWinSize();
        overlay->setPosition({ 5.f, winSize.height - 110.f });
        overlay->setZOrder(1000);
        m_uiLayer->addChild(overlay);

        return true;
    }
};

// ----------------------------------------------------------------------------
//  Logica de salto asistido
// ----------------------------------------------------------------------------
class $modify(AssistPlayer, PlayerObject) {
    struct Fields {
        bool pending = false;          // hay un clic guardado esperando suelo
        bool releaseQueued = false;    // el usuario solto el boton mientras esperaba
        int bufferLeft = 0;            // ticks restantes del buffer
        int ticksSinceGround = 9999;   // ticks desde que dejo el suelo
        bool usedSinceGround = false;  // ya se hizo un clic desde el ultimo suelo
        PlayerButton button = PlayerButton::Jump;
    };

    // Solo asistimos a los jugadores reales del PlayLayer
    bool isAssistedPlayer() {
        auto pl = PlayLayer::get();
        if (!pl) return false;
        return this == pl->m_player1 || this == pl->m_player2;
    }

    // Modos en los que el salto NO depende del suelo
    bool isAirControlMode() {
        return m_isShip || m_isBird || m_isDart || m_isSwing;
    }

    void clearBuffer() {
        m_fields->pending = false;
        m_fields->releaseQueued = false;
        m_fields->bufferLeft = 0;
    }

    void resetObject() {
        PlayerObject::resetObject();
        clearBuffer();
        m_fields->ticksSinceGround = 9999;
        m_fields->usedSinceGround = false;
    }

    bool pushButton(PlayerButton button) {
        if (g_mode == 0
            || button != PlayerButton::Jump
            || !isAssistedPlayer()
            || isAirControlMode()) {
            return PlayerObject::pushButton(button);
        }

        auto const& cfg = MODES[g_mode];
        auto f = m_fields.self();

        // 1) En el suelo: comportamiento normal.
        if (m_isOnGround) {
            f->usedSinceGround = true;
            clearBuffer();
            return PlayerObject::pushButton(button);
        }

        // 2) Coyote: acabas de dejar el suelo (sin saltar) y pulsas tarde.
        //    Concedemos el salto como si siguieras en el suelo.
        if (!f->usedSinceGround && f->ticksSinceGround <= cfg.coyoteTicks) {
            f->usedSinceGround = true;
            clearBuffer();

            m_isOnGround = true;                       // truco temporal
            bool res = PlayerObject::pushButton(button);
            m_isOnGround = false;

            g_assists++;
            g_lastEvent = "Coyote (+" + std::to_string(f->ticksSinceGround) + " ticks)";
            refreshUI();
            return res;
        }

        // 3) En el aire: dejamos que el juego procese el clic (orbes, buffer
        //    propio, etc.) y ademas lo guardamos por si aterrizas en <= N ticks.
        double velBefore = m_yVelocity;
        bool res = PlayerObject::pushButton(button);

        // Si el clic ya fue consumido (p.ej. un orbe te impulso hacia arriba),
        // no lo guardamos para evitar un salto fantasma al aterrizar.
        bool consumed = std::abs(m_yVelocity - velBefore) > 0.01;
        if (!consumed) {
            f->pending = true;
            f->releaseQueued = false;
            f->bufferLeft = cfg.bufferTicks;
            f->button = button;
        }
        return res;
    }

    bool releaseButton(PlayerButton button) {
        if (g_mode != 0 && button == PlayerButton::Jump && isAssistedPlayer()
            && m_fields->pending) {
            // Recordamos que fue un toque corto; se soltara tras ejecutarlo.
            m_fields->releaseQueued = true;
        }
        return PlayerObject::releaseButton(button);
    }

    void update(float dt) {
        PlayerObject::update(dt);

        if (!isAssistedPlayer()) return;

        auto f = m_fields.self();

        // Seguimiento de suelo (para coyote)
        if (m_isOnGround) {
            f->ticksSinceGround = 0;
            f->usedSinceGround = false;
        }
        else if (f->ticksSinceGround < 9999) {
            f->ticksSinceGround++;
        }

        if (g_mode == 0) {
            if (f->pending) clearBuffer();
            return;
        }

        if (!f->pending) return;

        // Frame exacto: primer tick en el que se puede saltar
        if (m_isOnGround) {
            bool wasReleased = f->releaseQueued;
            auto button = f->button;
            clearBuffer();

            f->usedSinceGround = true;
            PlayerObject::pushButton(button);
            if (wasReleased) {
                PlayerObject::releaseButton(button);
            }

            g_assists++;
            g_lastEvent = "Buffer ejecutado al aterrizar";
            refreshUI();
            return;
        }

        // Se acabo la ventana de tolerancia: se descarta el clic
        if (--f->bufferLeft <= 0) {
            clearBuffer();
        }
    }
};

// ----------------------------------------------------------------------------
//  Carga del mod
// ----------------------------------------------------------------------------
$on_mod(Loaded) {
#ifdef GEODE_IS_WINDOWS
    openConsoleWindow();
#endif
    refreshUI();
    log::info("Input Assist cargado. Pulsa F6 para cambiar de modo.");
}