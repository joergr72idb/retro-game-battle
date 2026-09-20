"""
pygame display loop - ported from
retro-tron-battle/server/tron_server.py's pygame_loop(). The HUD, side
panels, waiting-screen (scroll/highscore/tribute/greeting), countdown and
end overlays, and footer are all already game-agnostic and are unchanged
here. The one Tron-shaped piece, draw_trail(), is kept as a generic
draw_body() (mechanically identical: filled squares + a highlighted head -
Snake reuses it as-is for its bodies) plus a per-game draw_extra dispatch
for anything else a game wants to draw (e.g. Snake's food) - see
games/base.py's draw_extra().
"""

import colorsys
import math
import os
import time

import config
from engine.photo import event_logo_path
from engine.render import snapshot_render
from games import GAME_REGISTRY


def color_for(platform: str):
    return config.PLATFORM_COLORS.get(platform, config.DEFAULT_COLOR)


def pygame_loop():
    import pygame

    pygame.init()
    pygame.display.set_caption("Retro Game Battle")
    screen = pygame.display.set_mode((config.WINDOW_W, config.WINDOW_H))
    clock = pygame.time.Clock()
    font_big = pygame.font.SysFont("couriernew,monospace", 28, bold=True)
    font_huge = pygame.font.SysFont("couriernew,monospace", 96, bold=True)
    font_mid = pygame.font.SysFont("couriernew,monospace", 20, bold=True)
    font_small = pygame.font.SysFont("couriernew,monospace", 16)
    font_tiny = pygame.font.SysFont("couriernew,monospace", 14)
    tron_font_found = bool(config.SCROLL_FONT_PATH and os.path.exists(config.SCROLL_FONT_PATH))
    if not tron_font_found and config.SCROLL_FONT_PATH:
        print(f"[display] Scroll font not found: {config.SCROLL_FONT_PATH} - using default font")

    def load_tron_font(size):
        if tron_font_found:
            return pygame.font.Font(config.SCROLL_FONT_PATH, size)
        return pygame.font.SysFont("couriernew,monospace", size, bold=True)

    scroll_font = load_tron_font(config.SCROLL_FONT_SIZE)
    highscore_title_font = load_tron_font(84)
    highscore_row_font = load_tron_font(42)
    tribute_title_font = load_tron_font(40)
    tribute_caption_font = load_tron_font(24)
    tribute_url_font = load_tron_font(16)

    scroll_char_widths = [scroll_font.size(c)[0] for c in config.SCROLL_TEXT]
    scroll_total_width = sum(scroll_char_widths)
    scroll_x = config.WINDOW_W
    scroll_wave_phase = 0.0
    scroll_hue = 0.0

    waiting_mode = "scroll"
    scroll_loop_count = 0
    highscore_shown_until = 0.0
    tribute_shown_until = 0.0
    greeting_shown_until = 0.0

    image_cache = {}

    def load_scaled(path, max_w, max_h):
        if not path:
            return None
        key = (path, max_w, max_h)
        if key in image_cache:
            return image_cache[key]
        try:
            img = pygame.image.load(path)
            img = img.convert_alpha() if img.get_flags() else img.convert()
            iw, ih = img.get_size()
            scale = min(max_w / iw, max_h / ih)
            new_size = (max(1, int(iw * scale)), max(1, int(ih * scale)))
            scaled = pygame.transform.smoothscale(img, new_size)
        except Exception as e:
            print(f"[display] couldn't load image {path}: {e}")
            scaled = None
        image_cache[key] = scaled
        return scaled

    event_logo_surf = load_scaled(event_logo_path(), config.PANEL_W - 20, 70)
    if event_logo_surf is None:
        print(f"[display] club logo not found (expected: logo.jpg/.png/... in {config.LOGO_DIR})")
        try:
            actual_files = os.listdir(config.LOGO_DIR)
            print(f"[display] actually present in {config.LOGO_DIR}: {actual_files}")
        except Exception as e:
            print(f"[display] couldn't list {config.LOGO_DIR}: {e}")

    def draw_panel(x0, name, plat, photo_path, logo_path):
        color = color_for(plat)
        pygame.draw.rect(screen, config.HUD_BG_COLOR, (x0, config.HUD_HEIGHT, config.PANEL_W, config.GRID_H * config.CELL))
        pygame.draw.rect(screen, color, (x0, config.HUD_HEIGHT, config.PANEL_W, config.GRID_H * config.CELL), 2)

        y = config.HUD_HEIGHT + 12
        logo_surf = load_scaled(logo_path, config.PANEL_W - 20, 70)
        if logo_surf:
            rect = logo_surf.get_rect(centerx=x0 + config.PANEL_W // 2, top=y)
            screen.blit(logo_surf, rect)
            y = rect.bottom + 10
        else:
            y += 80

        photo_surf = load_scaled(photo_path, config.PANEL_W - 20, 220)
        if photo_surf:
            rect = photo_surf.get_rect(centerx=x0 + config.PANEL_W // 2, top=y)
            screen.blit(photo_surf, rect)
            y = rect.bottom + 10
        else:
            ph = pygame.Rect(x0 + 10, y, config.PANEL_W - 20, 180)
            pygame.draw.rect(screen, config.GRID_LINE_COLOR, ph)
            tip = font_tiny.render("no photo", True, config.DIM_TEXT_COLOR)
            screen.blit(tip, tip.get_rect(center=ph.center))
            y = ph.bottom + 10

        if name:
            label = font_small.render(f"{plat}", True, color)
            screen.blit(label, label.get_rect(centerx=x0 + config.PANEL_W // 2, top=y))
            y += 20
            label2 = font_small.render(name[:16], True, config.TEXT_COLOR)
            screen.blit(label2, label2.get_rect(centerx=x0 + config.PANEL_W // 2, top=y))

    def draw_highscore(wins, draws, games_played):
        cx = config.FIELD_X + (config.GRID_W * config.CELL) // 2
        top = config.HUD_HEIGHT + 40

        title = highscore_title_font.render("HIGH SCORES", True, (255, 220, 80))
        screen.blit(title, title.get_rect(centerx=cx, top=top))
        y = top + title.get_height() + 30

        ranked = sorted(wins.items(), key=lambda kv: -kv[1])
        if not ranked:
            line = highscore_row_font.render("NO GAMES PLAYED YET", True, config.DIM_TEXT_COLOR)
            screen.blit(line, line.get_rect(centerx=cx, top=y))
            y += line.get_height() + 10
        else:
            for rank, (plat, count) in enumerate(ranked, start=1):
                color = color_for(plat)
                text = f"{rank}.  {plat}   {count} WIN{'S' if count != 1 else ''}"
                line = highscore_row_font.render(text, True, color)
                screen.blit(line, line.get_rect(centerx=cx, top=y))
                y += line.get_height() + 14

        y += 20
        footer = font_small.render(
            f"Games played: {games_played}   Draws: {draws}", True, config.DIM_TEXT_COLOR)
        screen.blit(footer, footer.get_rect(centerx=cx, top=y))

    def wrap_text(text, font, max_width):
        words = text.split(" ")
        lines = []
        cur = ""
        for w in words:
            test = (cur + " " + w).strip()
            if not cur or font.size(test)[0] <= max_width:
                cur = test
            else:
                lines.append(cur)
                cur = w
        if cur:
            lines.append(cur)
        return lines

    def wrap_url(text, font, max_width):
        parts = []
        cur_part = ""
        for ch in text:
            cur_part += ch
            if ch == "/":
                parts.append(cur_part)
                cur_part = ""
        if cur_part:
            parts.append(cur_part)

        lines = []
        cur = ""
        for p in parts:
            test = cur + p
            if not cur or font.size(test)[0] <= max_width:
                cur = test
            else:
                lines.append(cur)
                cur = p
        if cur:
            lines.append(cur)
        return lines

    def draw_tribute():
        cx = config.FIELD_X + (config.GRID_W * config.CELL) // 2
        top = config.HUD_HEIGHT + 30

        title = tribute_title_font.render(config.TRIBUTE_TITLE, True, (255, 220, 80))
        screen.blit(title, title.get_rect(centerx=cx, top=top))

        n = len(config.TRIBUTE_ENTRIES)
        if n == 0:
            return
        col_w = (config.GRID_W * config.CELL) // n
        img_top = top + title.get_height() + 30
        img_max_w = col_w - 40
        img_max_h = 220

        for idx, (path, credit, url) in enumerate(config.TRIBUTE_ENTRIES):
            col_cx = config.FIELD_X + col_w * idx + col_w // 2

            img = load_scaled(path, img_max_w, img_max_h)
            if img:
                rect = img.get_rect(centerx=col_cx, top=img_top)
                screen.blit(img, rect)

            y = img_top + img_max_h + 16
            for line in wrap_text(credit, tribute_caption_font, col_w - 20):
                line_surf = tribute_caption_font.render(line, True, config.TEXT_COLOR)
                screen.blit(line_surf, line_surf.get_rect(centerx=col_cx, top=y))
                y += line_surf.get_height() + 4

            y += 8
            for line in wrap_url(url, tribute_url_font, col_w - 20):
                line_surf = tribute_url_font.render(line, True, config.DIM_TEXT_COLOR)
                screen.blit(line_surf, line_surf.get_rect(centerx=col_cx, top=y))
                y += line_surf.get_height() + 2

    def draw_greeting():
        cx = config.FIELD_X + (config.GRID_W * config.CELL) // 2
        y = config.HUD_HEIGHT + 40

        img = load_scaled(config.GREETING_IMAGE, config.GRID_W * config.CELL - 160, 340)
        if img:
            rect = img.get_rect(centerx=cx, top=y)
            screen.blit(img, rect)
            y = rect.bottom + 30
        else:
            y += 30

        for line in wrap_text(config.GREETING_TEXT, tribute_title_font, config.GRID_W * config.CELL - 80):
            line_surf = tribute_title_font.render(line, True, (255, 220, 80))
            screen.blit(line_surf, line_surf.get_rect(centerx=cx, top=y))
            y += line_surf.get_height() + 6

    def draw_body(cells, plat, head):
        color = color_for(plat)
        for (gx, gy) in cells:
            rect = (config.FIELD_X + gx * config.CELL + 1, config.HUD_HEIGHT + gy * config.CELL + 1,
                    config.CELL - 2, config.CELL - 2)
            pygame.draw.rect(screen, color, rect)
        if head:
            hx, hy = head
            rect = (config.FIELD_X + hx * config.CELL + 1, config.HUD_HEIGHT + hy * config.CELL + 1,
                    config.CELL - 2, config.CELL - 2)
            pygame.draw.rect(screen, (255, 255, 255), rect, 2)

    draw_ctx = {"pygame": pygame, "field_x": config.FIELD_X, "hud_height": config.HUD_HEIGHT, "cell": config.CELL}

    running = True
    while running:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False

        state = snapshot_render()

        screen.fill(config.HUD_BG_COLOR)

        title = font_big.render("RETRO GAME BATTLE", True, config.TEXT_COLOR)
        screen.blit(title, (16, 10))

        if state["phase"] == "playing":
            p1c = color_for(state["p1_plat"])
            p2c = color_for(state["p2_plat"])
            game_label = state["game_id"] or ""
            vs = font_mid.render(
                f"[{game_label}] {state['p1_plat']}/{state['p1_name']}  vs  {state['p2_plat']}/{state['p2_name']}",
                True, config.TEXT_COLOR)
            screen.blit(vs, (16, 46))
            pygame.draw.rect(screen, p1c, (16, 70, 16, 16))
            pygame.draw.rect(screen, p2c, (200, 70, 16, 16))
        else:
            msg = font_mid.render(state["status"], True, config.TEXT_COLOR)
            screen.blit(msg, (16, 46))

        wins_parts = []
        for plat, count in sorted(state["wins"].items(), key=lambda kv: -kv[1]):
            wins_parts.append(f"{plat}:{count}")
        wins_str = "  ".join(wins_parts) if wins_parts else "no wins yet"
        stats_line = font_small.render(
            f"Games played: {state['games_played']}   Wins - {wins_str}   Draws: {state['draws']}",
            True, config.DIM_TEXT_COLOR)
        screen.blit(stats_line, (16, 96))

        if event_logo_surf:
            rect = event_logo_surf.get_rect(right=config.WINDOW_W - 16, top=10)
            screen.blit(event_logo_surf, rect)

        if state["phase"] in ("countdown", "playing", "ended"):
            draw_panel(0, state["p1_name"], state["p1_plat"], state["photo1"], state["logo1"])
            draw_panel(config.FIELD_X + config.GRID_W * config.CELL, state["p2_name"], state["p2_plat"],
                       state["photo2"], state["logo2"])

        field_rect = pygame.Rect(config.FIELD_X, config.HUD_HEIGHT, config.GRID_W * config.CELL, config.GRID_H * config.CELL)
        pygame.draw.rect(screen, config.BG_COLOR, field_rect)
        for gx in range(config.GRID_W + 1):
            x = config.FIELD_X + gx * config.CELL
            pygame.draw.line(screen, config.GRID_LINE_COLOR, (x, config.HUD_HEIGHT), (x, config.WINDOW_H))
        for gy in range(config.GRID_H + 1):
            y = config.HUD_HEIGHT + gy * config.CELL
            pygame.draw.line(screen, config.GRID_LINE_COLOR, (config.FIELD_X, y), (config.FIELD_X + config.GRID_W * config.CELL, y))

        if state["phase"] == "waiting":
            if waiting_mode == "highscore":
                draw_highscore(state["wins"], state["draws"], state["games_played"])
                if time.monotonic() >= highscore_shown_until:
                    waiting_mode = "tribute"
                    tribute_shown_until = time.monotonic() + config.TRIBUTE_DURATION
            elif waiting_mode == "tribute":
                draw_tribute()
                if time.monotonic() >= tribute_shown_until:
                    waiting_mode = "greeting"
                    greeting_shown_until = time.monotonic() + config.GREETING_DURATION
            elif waiting_mode == "greeting":
                draw_greeting()
                if time.monotonic() >= greeting_shown_until:
                    waiting_mode = "scroll"
            else:
                scroll_x -= config.SCROLL_SPEED
                scroll_wave_phase += config.SCROLL_WAVE_SPEED
                scroll_hue = (scroll_hue + config.SCROLL_COLOR_SPEED) % 1.0
                if scroll_x < -scroll_total_width:
                    scroll_x = config.WINDOW_W
                    scroll_loop_count += 1
                    if scroll_loop_count >= config.SCROLL_LOOPS_BEFORE_HIGHSCORE:
                        scroll_loop_count = 0
                        waiting_mode = "highscore"
                        highscore_shown_until = time.monotonic() + config.HIGHSCORE_DURATION

                baseline = config.HUD_HEIGHT + (config.GRID_H * config.CELL) // 2
                cx = scroll_x
                for i, ch in enumerate(config.SCROLL_TEXT):
                    cw = scroll_char_widths[i]
                    if -cw <= cx <= config.WINDOW_W:
                        hue = (scroll_hue + i * 0.02) % 1.0
                        rr, gg, bb = colorsys.hsv_to_rgb(hue, 0.85, 1.0)
                        color = (int(rr * 255), int(gg * 255), int(bb * 255))
                        y_off = math.sin(scroll_wave_phase + i * config.SCROLL_WAVE_FREQ) * config.SCROLL_WAVE_AMPLITUDE
                        ch_surf = scroll_font.render(ch, True, color)
                        screen.blit(ch_surf, (cx, baseline + y_off))
                    cx += cw

        if state["phase"] in ("countdown", "playing", "ended"):
            draw_body(state["trail1"], state["p1_plat"], state["pos1"])
            draw_body(state["trail2"], state["p2_plat"], state["pos2"])
            game = GAME_REGISTRY.get(state["game_id"])
            if game is not None:
                game.draw_extra(screen, state, draw_ctx)

        if state["phase"] == "countdown" and state["countdown"]:
            cd_color = (120, 255, 140) if state["countdown"] == "Go, get in there! Move!" else (255, 220, 80)
            cd_font = font_huge if len(state["countdown"]) <= 2 else font_big
            big = cd_font.render(state["countdown"], True, cd_color)
            bg_rect = big.get_rect(center=(config.FIELD_X + (config.GRID_W * config.CELL) // 2,
                                            config.HUD_HEIGHT + (config.GRID_H * config.CELL) // 2))
            pad = pygame.Rect(bg_rect.x - 20, bg_rect.y - 14, bg_rect.width + 40, bg_rect.height + 28)
            pygame.draw.rect(screen, (0, 0, 0), pad)
            pygame.draw.rect(screen, cd_color, pad, 3)
            screen.blit(big, bg_rect)

        if state["phase"] == "ended":
            big = font_big.render(state["status"], True, (255, 220, 80))
            bg_rect = big.get_rect(center=(config.FIELD_X + (config.GRID_W * config.CELL) // 2,
                                            config.HUD_HEIGHT + (config.GRID_H * config.CELL) // 2))
            pad = pygame.Rect(bg_rect.x - 12, bg_rect.y - 8, bg_rect.width + 24, bg_rect.height + 16)
            pygame.draw.rect(screen, (0, 0, 0), pad)
            pygame.draw.rect(screen, (255, 220, 80), pad, 2)
            screen.blit(big, bg_rect)

        footer_y = config.HUD_HEIGHT + config.GRID_H * config.CELL
        pygame.draw.rect(screen, config.HUD_BG_COLOR, (0, footer_y, config.WINDOW_W, config.FOOTER_HEIGHT))
        row_h = config.FOOTER_HEIGHT // 2
        row1_y = footer_y + row_h // 2
        row2_y = footer_y + row_h + row_h // 2

        footer_text = font_tiny.render(
            "This game was generated with AI assistance.",
            True, config.DIM_TEXT_COLOR)
        screen.blit(footer_text, footer_text.get_rect(center=(config.WINDOW_W // 2, row1_y)))
        version_text = font_tiny.render(f"build {config.SERVER_BUILD}", True, config.DIM_TEXT_COLOR)
        screen.blit(version_text, version_text.get_rect(right=config.WINDOW_W - 10, centery=row1_y))

        credits_text = font_tiny.render(
            "Ported from Retro Tron Battle (Classic Computing 2026)  ·  "
            "Flynn font: Neale Davidson (Pixel Sagas)  ·  "
            "WiC64 HTTP-Requester: Andreas Beermann (andi6510)",
            True, config.DIM_TEXT_COLOR)
        screen.blit(credits_text, credits_text.get_rect(center=(config.WINDOW_W // 2, row2_y)))

        pygame.display.flip()
        clock.tick(30)

    pygame.quit()
