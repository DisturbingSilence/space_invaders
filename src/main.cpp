#include <ndc/window.h>
#include <ndc/gl.h>
#include <ndc/gl_utils.h>

#include <iostream>
#include <cstdint>
#include <algorithm>
#include <vector>
#include <cassert>
#include <random>

int move_dir = 0;
bool fire_pressed = 0;

void key_callback(ndc_window* win,ndc_input_key_t key,uint32_t,uint32_t,ndc_input_action_t action)
{
	if(action != NDC_PRESS) return;
	switch(key)
	{
		case NDC_KEY_ESCAPE: win->alive = false; return;
		case NDC_KEY_D: move_dir += 1; return;
		case NDC_KEY_A: move_dir -= 1; return;
		case NDC_KEY_SPACE: fire_pressed = true; return;
		default: return;
	};
}
void size_callback(ndc_window*,std::uint32_t w,std::uint32_t h)
{
	glViewport(0,0,w,h);
}



struct Buffer
{
    std::uint32_t width{}, height{};
    std::vector<std::uint32_t> data;
    Buffer(std::uint32_t w,std::uint32_t h) :
    width{w},height{h},data(w*h)
    {
    }
};

struct Sprite
{
    std::uint32_t width{}, height{};
    std::vector<std::uint8_t> data;
    Sprite(std::uint32_t w, std::uint32_t h) : 
    width{w}, height{h}
    {}
    Sprite(const Sprite& other) = default;
    Sprite& operator=(const Sprite& other) = default;
};

struct Alien
{
    std::uint32_t x{}, y{};
    std::uint8_t type{};
};

struct Bullet
{
    std::uint32_t x{}, y{};
    std::int32_t dir{};
};

struct Player
{
    std::uint32_t x{}, y{};
    std::uint32_t life{3};
};


struct Game
{
    std::uint32_t width{}, height{},num_bullets{};
    std::vector<Alien> aliens;
    Player player;
    std::vector<Bullet> bullets;
    Game(std::uint32_t w,std::uint32_t h,std::uint32_t num_bullets,std::uint32_t num_aliens) : 
    width{w},height{h},
    aliens(num_aliens),
    bullets(num_bullets)
    {
    }
};

struct SpriteAnimation
{
    bool loop{};
    std::uint32_t num_frames{};
    std::uint32_t frame_duration{};
    std::uint32_t time{};
    std::vector<Sprite*> frames;
    SpriteAnimation(bool loop,std::uint32_t fr,std::uint32_t dur,std::uint32_t time) :
    loop{loop},
    num_frames{fr},
    frame_duration{dur},
    time{time},
    frames(num_frames)
    {}
};

enum AlienType: uint8_t
{
    ALIEN_DEAD   = 0,
    ALIEN_TYPE_A = 1,
    ALIEN_TYPE_B = 2,
    ALIEN_TYPE_C = 3
};

void buffer_clear(Buffer& buffer, uint32_t color)
{
	std::ranges::fill(buffer.data,color);
}
bool collision_check(const Sprite& sp_a,std::uint32_t x_a,std::uint32_t y_a,const Sprite& sp_b,std::uint32_t x_b,std::uint32_t y_b)
{
    return (x_a < x_b + sp_b.width && x_a + sp_a.width > x_b && y_a < y_b + sp_b.height && y_a + sp_a.height > y_b);
}

void draw_sprite(Buffer& buffer, const Sprite& sprite,std::uint32_t x, std::uint32_t y,std::uint32_t color)
{
    for(std::uint32_t xi = 0; xi < sprite.width; ++xi)
    {
        for(std::uint32_t yi = 0; yi < sprite.height; ++yi)
        {
            if(sprite.data[yi * sprite.width + xi] && (sprite.height - 1 + y - yi) < buffer.height &&
               (x + xi) < buffer.width)
            {
                buffer.data[(sprite.height - 1 + y - yi) * buffer.width + (x + xi)] = color;
            }
        }
    }
}


void draw_glyphs(Buffer& buffer,const Sprite& spritesheet,std::uint32_t number,std::uint32_t x, std::uint32_t y,std::uint32_t color)
{
    std::uint8_t digits[64];
    std::uint32_t num_digits = 0;

    std::uint32_t current_number = number;
    do
    {
        digits[num_digits++] = current_number % 10;
        current_number = current_number / 10;
    }
    while(current_number > 0);

    size_t xp = x;
    size_t stride = spritesheet.width * spritesheet.height;
    Sprite sprite = spritesheet;
    for(size_t i = 0; i < num_digits; ++i)
    {
        uint8_t digit = digits[num_digits - i - 1];
        sprite.data.assign(spritesheet.data.begin() + digit * stride, spritesheet.data.begin() + (digit+1) * stride);
        draw_sprite(buffer, sprite, xp, y, color);
        xp += sprite.width + 1;
    }
}

void draw_glyphs(Buffer& buffer,const Sprite& spritesheet,std::string_view text,std::uint32_t x,std::uint32_t y,std::uint32_t color)
{
    size_t xp = x;
    size_t stride = spritesheet.width * spritesheet.height;
    Sprite sprite = spritesheet;

    for(char c : text)
    {
        char character = c - 32;
        if(character < 0 || character >= 65) continue;

        sprite.data.assign(spritesheet.data.begin() + character * stride, spritesheet.data.begin() + (character+1) * stride); 
        draw_sprite(buffer, sprite, xp, y, color);
        xp += sprite.width + 1;
    }
}


std::uint32_t rgb_to_u32(std::uint8_t r,std::uint8_t g,std::uint8_t b)
{
    return (r << 24) | (g << 16) | (b << 8) | 255;
}
static std::string fragment_shader = 
R"(//F
#version 460

layout(location = 0) uniform sampler2D img;
layout(location = 0) in vec2 i_uv;
layout(location = 0) out vec3 o_color;
out vec3 outColor;
void main()
{
	o_color = texture(img,i_uv).rgb;
}
)";
static std::string vertex_shader = 
R"(//V
#version 460
layout(location = 0) out vec2 o_uv;

void main()
{
	o_uv.x = (gl_VertexID == 2)? 2.0: 0.0;
	o_uv.y = (gl_VertexID == 1)? 2.0: 0.0;
	gl_Position = vec4(2.0 * o_uv - 1.0, 0.0, 1.0);
}
)";
double random_double()
{
	static std::mt19937_64 mt{std::random_device{}()};
	static std::uniform_real_distribution<double> dist(0,1);
	return dist(mt);
}
void set_debug_callback();
std::vector<Sprite> create_alien_sprites();
std::vector<Sprite> create_glyph_spritesheets();

int main()
{
   	const size_t buffer_width = 224;
    const size_t buffer_height = 256;

    ndc_window* win = ndc_create_window("space invaders",buffer_width * 2,buffer_height*2);
    ndc_set_key_callback(win,key_callback);
    ndc_set_size_callback(win,size_callback);
#ifdef NDEBUG
    set_debug_callback();
#endif
    ndc_show_window(win);
 
    glClearColor(1.0, 0.0, 0.0, 1.0);

    Buffer buffer(buffer_width,buffer_height);

    buffer_clear(buffer, 0);

    std::uint32_t buffer_texture = 0;
    glCreateTextures(GL_TEXTURE_2D, 1, &buffer_texture);
	glTextureParameteri(buffer_texture, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTextureParameteri(buffer_texture, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTextureStorage2D(buffer_texture, 1, GL_RGBA8,buffer.width, buffer.height);
	glTextureSubImage2D(buffer_texture, 0, 0, 0, buffer.width, buffer.height, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8, buffer.data.data());

    std::uint32_t vao = 0;
    glCreateVertexArrays(1, &vao);

    auto shader = ndc_compile_program(2,fragment_shader.data(),vertex_shader.data());

    glUseProgram(shader);

    glBindTextureUnit(0,buffer_texture);
    glBindVertexArray(vao);
 
    auto alien_sprites = create_alien_sprites();
    auto spritesheets = create_glyph_spritesheets();


    Sprite alien_death_sprite(13,7);
    alien_death_sprite.data =
    {
        0,1,0,0,1,0,0,0,1,0,0,1,0,
        0,0,1,0,0,1,0,1,0,0,1,0,0,
        0,0,0,1,0,0,0,0,0,1,0,0,0, 
        1,1,0,0,0,0,0,0,0,0,0,1,1, 
        0,0,0,1,0,0,0,0,0,1,0,0,0, 
        0,0,1,0,0,1,0,1,0,0,1,0,0, 
        0,1,0,0,1,0,0,0,1,0,0,1,0 
    };

    Sprite player_sprite(11,7);
    player_sprite.data = 
    {
        0,0,0,0,0,1,0,0,0,0,0,
        0,0,0,0,1,1,1,0,0,0,0,
        0,0,0,0,1,1,1,0,0,0,0, 
        0,1,1,1,1,1,1,1,1,1,0,
        1,1,1,1,1,1,1,1,1,1,1, 
        1,1,1,1,1,1,1,1,1,1,1, 
        1,1,1,1,1,1,1,1,1,1,1, 
    };

    Sprite player_bullet_sprite(1,3);
    player_bullet_sprite.data =
    {
        1, 1, 1
    };
    std::vector<Sprite> alien_bullet_sprite;
    alien_bullet_sprite.emplace_back(3,7);
    alien_bullet_sprite.emplace_back(3,7);

    alien_bullet_sprite[0].data =
    {
        0,1,0,
        1,0,0,
        0,1,0,
        0,0,1,
        0,1,0,
        1,0,0,
        0,1,0,
    };
    alien_bullet_sprite[1].data = 
    {
        0,1,0,
        0,0,1,
        0,1,0,
        1,0,0,
        0,1,0,
        0,0,1,
        0,1,0,
    };

    SpriteAnimation alien_bullet_animation(true,2,5,0);

    alien_bullet_animation.frames[0] = &alien_bullet_sprite[0];
    alien_bullet_animation.frames[1] = &alien_bullet_sprite[1];

    std::vector<SpriteAnimation> alien_animation;
    std::uint32_t alien_update_frequency = 120;
    alien_animation.emplace_back(true,2,alien_update_frequency,0);
    alien_animation.emplace_back(true,2,alien_update_frequency,0);
    alien_animation.emplace_back(true,2,alien_update_frequency,0); 

    for(size_t i = 0; i < 3; ++i)
    {
        alien_animation[i].frames[0] = &alien_sprites[2 * i];
        alien_animation[i].frames[1] = &alien_sprites[2 * i + 1];
    }

    Game game(buffer.width,buffer.height,128,55);


    game.player.x = 110;
    game.player.y = 32;

    std::uint32_t alien_swarm_position = 24;
    std::uint32_t alien_swarm_max_position = game.width - 16 * 11 - 3;

    std::uint32_t aliens_killed = 0;
    std::uint32_t alien_update_timer = 0;

    for(std::uint32_t xi = 0; xi < 11; ++xi)
    {
        for(std::uint32_t yi = 0; yi < 5; ++yi)
        {
            Alien& alien = game.aliens[xi * 5 + yi];
            alien.type = (5 - yi) / 2 + 1;

            const Sprite& sprite = alien_sprites[2 * (alien.type - 1)];

            alien.x = 16 * xi + alien_swarm_position + (alien_death_sprite.width - sprite.width)/2;
            alien.y = 17 * yi + 128;
        }
    }
    std::vector<std::uint8_t> death_counters(game.aliens.size());
    std::ranges::fill(death_counters,10);

    std::uint32_t clear_color = rgb_to_u32(0, 0, 0);
    std::uint32_t ui_color = rgb_to_u32(255,255,255);
    std::uint32_t player_color = rgb_to_u32(30,20,240);

    std::int32_t alien_move_dir = 4;
    std::uint32_t score = 0;

  
    std::int32_t player_move_dir = 0;

    
    while (win->alive)
    {
        buffer_clear(buffer, clear_color);

        if(game.player.life == 0)
        {
            draw_glyphs(buffer, spritesheets[0], "GAME OVER", game.width / 2 - 30, game.height / 2, ui_color);
            draw_glyphs(buffer, spritesheets[0], "SCORE", 4, game.height - spritesheets[0].height - 7, ui_color);
            draw_glyphs(buffer, spritesheets[1], score, 4 + 2 * spritesheets[1].width, game.height - 2 * spritesheets[1].height - 12, ui_color);

            glTextureSubImage2D(buffer_texture, 0, 0, 0, buffer.width, buffer.height,GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,buffer.data.data());
            glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

            ndc_swap_buffers(win);
            ndc_poll_events(win);
            continue;
        }

        draw_glyphs(buffer, spritesheets[0], "SCORE", 4, game.height - spritesheets[0].height - 7, ui_color);
        draw_glyphs(buffer, spritesheets[1], score, 4 + 2 * spritesheets[1].width, game.height - 2 * spritesheets[1].height - 12,ui_color);

        draw_glyphs(buffer, spritesheets[1], game.player.life, 4, 7, ui_color);
        std::uint32_t xp =  11 + spritesheets[1].width;
        for(std::uint32_t i = 0; i < game.player.life; ++i)
        {
            draw_sprite(buffer, player_sprite, xp, 7, player_color);
            xp += player_sprite.width + 2;
        }

        for(std::uint32_t i = 0; i < game.width; ++i)
        {
            buffer.data[game.width * 16 + i] = ui_color;
        }

        for(std::uint32_t ai = 0; ai < game.aliens.size(); ++ai)
        {
            if(death_counters[ai] == 0) continue;

            const Alien& alien = game.aliens[ai];
            if(alien.type == ALIEN_DEAD)
            {
                draw_sprite(buffer, alien_death_sprite, alien.x, alien.y, rgb_to_u32(128, 0, 0));
            }
            else
            {
            	std::uint32_t color = {};
            	switch(alien.type)
	            {
	            	case ALIEN_TYPE_A: color = rgb_to_u32(2,252,2) ; break;
	            	case ALIEN_TYPE_B: color = rgb_to_u32(1,255,255); break;
	            	case ALIEN_TYPE_C: color = rgb_to_u32(255,0,220); break;
	            };
                const SpriteAnimation& animation = alien_animation[alien.type - 1];
                std::uint32_t current_frame = animation.time / animation.frame_duration;
                const Sprite& sprite = *animation.frames[current_frame];
                draw_sprite(buffer, sprite, alien.x, alien.y, color);
            }
        }

        for(std::uint32_t bi = 0; bi < game.num_bullets; ++bi)
        {
            const Bullet& bullet = game.bullets[bi];
            const Sprite* sprite{};
            if(bullet.dir > 0) sprite = &player_bullet_sprite;
            else
            {
                std::uint32_t cf = alien_bullet_animation.time / alien_bullet_animation.frame_duration;
                sprite = &alien_bullet_sprite[cf];
            }
            draw_sprite(buffer, *sprite, bullet.x, bullet.y, rgb_to_u32(128, 128, 0));
        }

        draw_sprite(buffer, player_sprite, game.player.x, game.player.y, player_color);
        glTextureSubImage2D(buffer_texture, 0, 0, 0, buffer.width, buffer.height,GL_RGBA, GL_UNSIGNED_INT_8_8_8_8,buffer.data.data());
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        ndc_swap_buffers(win);

        for(std::uint32_t bi = 0; bi < game.num_bullets; ++bi)
        {
            game.bullets[bi].y += game.bullets[bi].dir;
            if(game.bullets[bi].y >= game.height || game.bullets[bi].y < player_bullet_sprite.height)
            {
                game.bullets[bi] = game.bullets[game.num_bullets - 1];
                --game.num_bullets;
                continue;
            }
            // enemy bullet
            if(game.bullets[bi].dir < 0)
            {
                bool collide = collision_check(
                    alien_bullet_sprite[0], game.bullets[bi].x, game.bullets[bi].y,
                    player_sprite, game.player.x, game.player.y
                );

                if(collide)
                {
                    --game.player.life;
                    game.bullets[bi] = game.bullets[game.num_bullets--];
                    --game.num_bullets;
                    break;
                }
            }
            // player bullet
            else
            {
                for(std::uint32_t bj = 0; bj < game.num_bullets; ++bj)
                {
                    if(bi == bj) continue;

                    bool collide = collision_check(
                        player_bullet_sprite, game.bullets[bi].x, game.bullets[bi].y,
                        alien_bullet_sprite[0], game.bullets[bj].x, game.bullets[bj].y
                    );

                    if(collide)
                    {
                        if(bj == game.num_bullets - 1)
                        {
                            game.bullets[bi] = game.bullets[game.num_bullets - 2];
                        }
                        else if(bi == game.num_bullets - 1)
                        {
                            game.bullets[bj] = game.bullets[game.num_bullets - 2];
                        }
                        else
                        {
                            game.bullets[(bi < bj)? bi: bj] = game.bullets[game.num_bullets - 1];
                            game.bullets[(bi < bj)? bj: bi] = game.bullets[game.num_bullets - 2];
                        }
                        game.num_bullets -= 2;
                        break;
                    }
                }

                for(std::uint32_t ai = 0; ai < game.aliens.size(); ++ai)
                {
                    const Alien& alien = game.aliens[ai];
                    if(alien.type == ALIEN_DEAD) continue;

                    const SpriteAnimation& animation = alien_animation[alien.type - 1];
                    std::uint32_t current_frame = animation.time / animation.frame_duration;
                    const Sprite& alien_sprite = *animation.frames[current_frame];
                    bool collide = collision_check(
                        player_bullet_sprite, game.bullets[bi].x, game.bullets[bi].y,
                        alien_sprite, alien.x, alien.y
                    );

                    if(collide)
                    {
                        score += 10 * (4 - game.aliens[ai].type);
                        game.aliens[ai].type = ALIEN_DEAD;
                        game.aliens[ai].x -= (alien_death_sprite.width - alien_sprite.width)/2;
                        game.bullets[bi] = game.bullets[game.num_bullets - 1];
                        --game.num_bullets;
                        ++aliens_killed;
                        if(aliens_killed % 15 == 0)
                    	{
                    		alien_update_frequency /= 2;
				            for(size_t i = 0; i < 3; ++i)
				            {
				                alien_animation[i].frame_duration = alien_update_frequency;
				            }
                    	}
                        break;
                    }
                }
            }
        }
        for(std::uint32_t ai = 0; ai < game.aliens.size(); ++ai)
        {
            const Alien& alien = game.aliens[ai];
            if(alien.type == ALIEN_DEAD && death_counters[ai])
            {
                --death_counters[ai];
            }
        }

        if(alien_update_timer >= alien_update_frequency)
        {
            alien_update_timer = 0;

            if(static_cast<std::int32_t>(alien_swarm_position) + alien_move_dir < 0)
            {
                alien_move_dir *= -1;
                for(std::uint32_t ai = 0; ai < game.aliens.size(); ++ai)
                {
                    Alien& alien = game.aliens[ai];
                    alien.y -= 8;
                }
            }
            else if(alien_swarm_position > alien_swarm_max_position - alien_move_dir)
            {
                alien_move_dir *= -1;
            }
            alien_swarm_position += alien_move_dir;

            for(std::uint32_t ai = 0; ai < game.aliens.size(); ++ai)
            {
                Alien& alien = game.aliens[ai];
                alien.x += alien_move_dir;
            }

            if(aliens_killed < game.aliens.size())
            {
                std::uint32_t rai = game.aliens.size() * random_double();
                while(game.aliens[rai].type == ALIEN_DEAD)
                {
                    rai = game.aliens.size() * random_double();
                }
                const Sprite& alien_sprite = *alien_animation[game.aliens[rai].type - 1].frames[0];
                game.bullets[game.num_bullets].x = game.aliens[rai].x + alien_sprite.width / 2;
                game.bullets[game.num_bullets].y = game.aliens[rai].y - alien_bullet_sprite[0].height;
                game.bullets[game.num_bullets].dir = -2;
                ++game.num_bullets;
            }
        }
        for(std::uint32_t i = 0; i < 3; ++i)
        {
            ++alien_animation[i].time;
            if(alien_animation[i].time >= alien_animation[i].num_frames * alien_animation[i].frame_duration)
            {
                alien_animation[i].time = 0;
            }
        }
        ++alien_bullet_animation.time;
        if(alien_bullet_animation.time >= alien_bullet_animation.num_frames * alien_bullet_animation.frame_duration)
        {
            alien_bullet_animation.time = 0;
        }

        ++alien_update_timer;
        player_move_dir = 2 * move_dir;

        if(player_move_dir != 0)
        {
            if(game.player.x + player_sprite.width + player_move_dir >= game.width)
            {
                game.player.x = game.width - player_sprite.width;
            }
            else if((int)game.player.x + player_move_dir <= 0)
            {
                game.player.x = 0;
            }
            else game.player.x += player_move_dir;
        }

        if(aliens_killed < game.aliens.size())
        {
            std::uint32_t ai = 0;
            while(game.aliens[ai].type == ALIEN_DEAD) ++ai;
            const Sprite& sprite = alien_sprites[2 * (game.aliens[ai].type - 1)];
            std::uint32_t pos = game.aliens[ai].x - (alien_death_sprite.width - sprite.width)/2;
            if(pos > alien_swarm_position) alien_swarm_position = pos;

            ai = game.aliens.size() - 1;
            while(game.aliens[ai].type == ALIEN_DEAD) --ai;
            pos = game.width - game.aliens[ai].x - 13 + pos;
            if(pos > alien_swarm_max_position) alien_swarm_max_position = pos;
        }
        else
        {
            alien_update_frequency = 120;
            alien_swarm_position = 24;

            aliens_killed = 0;
            alien_update_timer = 0;

            alien_move_dir = 4;

            for(size_t xi = 0; xi < 11; ++xi)
            {
                for(size_t yi = 0; yi < 5; ++yi)
                {
                    size_t ai = xi * 5 + yi;

                    death_counters[ai] = 10;

                    Alien& alien = game.aliens[ai];
                    alien.type = (5 - yi) / 2 + 1;

                    const Sprite& sprite = alien_sprites[2 * (alien.type - 1)];

                    alien.x = 16 * xi + alien_swarm_position + (alien_death_sprite.width - sprite.width)/2;
                    alien.y = 17 * yi + 128;
                }
            }
        }
        if(fire_pressed && game.num_bullets < game.bullets.size())
        {
            game.bullets[game.num_bullets].x = game.player.x + player_sprite.width / 2;
            game.bullets[game.num_bullets].y = game.player.y + player_sprite.height;
            game.bullets[game.num_bullets].dir = 2;
            ++game.num_bullets;
        }
        fire_pressed = false;

        ndc_poll_events(win);
    }
    glDeleteTextures(1,&buffer_texture);
    glDeleteProgram(shader);
    glDeleteVertexArrays(1, &vao);

    ndc_destroy_window(win);

}
std::vector<Sprite> create_alien_sprites()
{
	std::vector<Sprite> alien_sprites;
    alien_sprites.emplace_back(8,8);
    alien_sprites.emplace_back(8,8);
    alien_sprites.emplace_back(11,8);
    alien_sprites.emplace_back(11,8);
    alien_sprites.emplace_back(12,8);
    alien_sprites.emplace_back(12,8);

    alien_sprites[0].data =
    {
        0,0,0,1,1,0,0,0,
        0,0,1,1,1,1,0,0, 
        0,1,1,1,1,1,1,0, 
        1,1,0,1,1,0,1,1,
        1,1,1,1,1,1,1,1, 
        0,1,0,1,1,0,1,0,
        1,0,0,0,0,0,0,1, 
        0,1,0,0,0,0,1,0  
    };
    alien_sprites[1].data = 
    {
        0,0,0,1,1,0,0,0,
        0,0,1,1,1,1,0,0, 
        0,1,1,1,1,1,1,0,
        1,1,0,1,1,0,1,1,
        1,1,1,1,1,1,1,1,
        0,0,1,0,0,1,0,0,
        0,1,0,1,1,0,1,0, 
        1,0,1,0,0,1,0,1  
    };
    alien_sprites[2].data = 
    {
        0,0,1,0,0,0,0,0,1,0,0,
        0,0,0,1,0,0,0,1,0,0,0, 
        0,0,1,1,1,1,1,1,1,0,0, 
        0,1,1,0,1,1,1,0,1,1,0,
        1,1,1,1,1,1,1,1,1,1,1,
        1,0,1,1,1,1,1,1,1,0,1,
        1,0,1,0,0,0,0,0,1,0,1,
        0,0,0,1,1,0,1,1,0,0,0 
    };
    alien_sprites[3].data =
    {
        0,0,1,0,0,0,0,0,1,0,0,
        1,0,0,1,0,0,0,1,0,0,1,
        1,0,1,1,1,1,1,1,1,0,1,
        1,1,1,0,1,1,1,0,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1, 
        0,1,1,1,1,1,1,1,1,1,0,
        0,0,1,0,0,0,0,0,1,0,0,
        0,1,0,0,0,0,0,0,0,1,0 
    };
    alien_sprites[4].data = 
    {
        0,0,0,0,1,1,1,1,0,0,0,0,
        0,1,1,1,1,1,1,1,1,1,1,0,
        1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,0,0,1,1,0,0,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1, 
        0,0,0,1,1,0,0,1,1,0,0,0,
        0,0,1,1,0,1,1,0,1,1,0,0, 
        1,1,0,0,0,0,0,0,0,0,1,1
    };
    alien_sprites[5].data = 
    {
        0,0,0,0,1,1,1,1,0,0,0,0,
        0,1,1,1,1,1,1,1,1,1,1,0,
        1,1,1,1,1,1,1,1,1,1,1,1,
        1,1,1,0,0,1,1,0,0,1,1,1,
        1,1,1,1,1,1,1,1,1,1,1,1, 
        0,0,1,1,1,0,0,1,1,1,0,0,
        0,1,1,0,0,1,1,0,0,1,1,0,
        0,0,1,1,0,0,0,0,1,1,0,0 
    };
    return alien_sprites;
}
std::vector<Sprite> create_glyph_spritesheets()
{
    Sprite text(5,7);
    text.data = 
    {
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
        0,1,0,1,0,0,1,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,1,0,1,0,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,1,1,1,1,1,0,1,0,1,0,0,1,0,1,0,
        0,0,1,0,0,0,1,1,1,0,1,0,1,0,0,0,1,1,1,0,0,0,1,0,1,0,1,1,1,0,0,0,1,0,0,
        1,1,0,1,0,1,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,1,1,0,1,0,1,1,
        0,1,1,0,0,1,0,0,1,0,1,0,0,1,0,0,1,1,0,0,1,0,0,1,0,1,0,0,0,1,0,1,1,1,1,
        0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,
        1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
        0,0,1,0,0,1,0,1,0,1,0,1,1,1,0,0,0,1,0,0,0,1,1,1,0,1,0,1,0,1,0,0,1,0,0,
        0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,
        0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,

        0,1,1,1,0,1,0,0,0,1,1,0,0,1,1,1,0,1,0,1,1,1,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,0,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,0,0,0,0,1,0,0,1,1,0,0,1,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,1,0,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,0,0,1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,0,1,1,1,1,1,0,0,0,1,0,0,0,0,1,0,
        1,1,1,1,1,1,0,0,0,0,1,1,1,1,0,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,0,1,1,1,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,

        0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,1,0,0,
        0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,
        0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,1,1,1,1,1,0,0,0,0,0,0,0,0,0,0,
        1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,1,0,1,1,1,0,1,1,1,0,1,0,0,1,0,0,0,1,0,1,1,1,0,

        0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,
        1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,1,1,1,1,1,0,0,0,0,1,0,0,0,0,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,1,0,1,1,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,1,1,1,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,
        0,1,1,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,1,1,1,0,
        0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,1,0,1,0,1,0,0,1,1,0,0,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
        1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,1,1,1,1,
        1,0,0,0,1,1,1,0,1,1,1,0,1,0,1,1,0,1,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,1,1,0,0,1,1,0,1,0,1,1,0,0,1,1,1,0,0,0,1,1,0,0,0,1,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,0,0,1,1,0,1,1,1,1,
        1,1,1,1,0,1,0,0,0,1,1,0,0,0,1,1,1,1,1,0,1,0,1,0,0,1,0,0,1,0,1,0,0,0,1,
        0,1,1,1,0,1,0,0,0,1,1,0,0,0,0,0,1,1,1,0,1,0,0,0,1,0,0,0,0,1,0,1,1,1,0,
        1,1,1,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,1,1,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,
        1,0,0,0,1,1,0,0,0,1,1,0,0,0,1,1,0,1,0,1,1,0,1,0,1,1,1,0,1,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,1,0,0,0,1,
        1,0,0,0,1,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,
        1,1,1,1,1,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,1,1,1,1,1,

        0,0,0,1,1,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,1,
        0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,1,0,
        1,1,0,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1,0,0,1,1,0,0,0,
        0,0,1,0,0,0,1,0,1,0,1,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
        0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,1,1,
        0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0
    };

    Sprite numbers(5,7);
    numbers.data.assign(text.data.begin() + 16*35,text.data.end());
    return {text,numbers};
}
void set_debug_callback()
{
	glEnable(GL_DEBUG_OUTPUT);
	glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
	glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DONT_CARE, 0, nullptr, GL_TRUE);
	glDebugMessageCallback([](
		std::uint32_t src, 
		std::uint32_t type,
		std::uint32_t id, 
		std::uint32_t severity, int, const char* msg, const void*) -> void
	{
		if (id == 131169 || 
        id == 131185 ||
        id == 131218 || 
        id == 131204 || 
        id == 131222 ||
        id == 131154 ||
        id == 0         
      )
      return;
		auto srcOut = [=]() 
		{
			switch (src)
			{
				case GL_DEBUG_SOURCE_API: 			  return "API";
				case GL_DEBUG_SOURCE_OTHER: 		  return "OTHER";
				case GL_DEBUG_SOURCE_THIRD_PARTY: 	  return "THIRD PARTY";
				case GL_DEBUG_SOURCE_APPLICATION: 	  return "APPLICATION";
				case GL_DEBUG_SOURCE_WINDOW_SYSTEM:   return "WINDOW SYSTEM";
				case GL_DEBUG_SOURCE_SHADER_COMPILER: return "SHADER COMPILER";
				
				default : return "UNKNOWN";
			}
		}();

		auto typeOut = [=]() 
		{
			switch (type)
			{
				case GL_DEBUG_TYPE_ERROR: 				return "ERROR";
				case GL_DEBUG_TYPE_OTHER: 				return "OTHER";
				case GL_DEBUG_TYPE_MARKER: 			 	return "MARKER";
				case GL_DEBUG_TYPE_PORTABILITY: 		return "PORTABILITY";
				case GL_DEBUG_TYPE_PERFORMANCE: 		return "PERFORMANCE";
				case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:  return "UNDEFINED_BEHAVIOR";
				case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR: return "DEPRECATED_BEHAVIOR";
				
				default : return "UNKNOWN";
			}
		}();

		auto severityOut = [=]() 
		{
			switch (severity) 
			{
				case GL_DEBUG_SEVERITY_LOW: 		 return "LOW";
				case GL_DEBUG_SEVERITY_HIGH: 		 return "HIGH";
				case GL_DEBUG_SEVERITY_MEDIUM: 		 return "MEDIUM";
				case GL_DEBUG_SEVERITY_NOTIFICATION: return "NOTIFICATION";
				
				default : return "UNKNOWN";
			}
		}();
		printf("[%d | %s | %s | %s] %s\n",id,severityOut,typeOut,srcOut,msg);
	}, nullptr);
}