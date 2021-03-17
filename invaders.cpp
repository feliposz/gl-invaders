#include <stdint.h>
#include <math.h>
#include <assert.h>
#include <windows.h>
#include <GL/gl.h> 
#include "invaders.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

const float TAU = 6.28318530717958646f;

// Globals

int key_left = 0;
int key_right = 0;
int key_up = 0;
int key_down = 0;

bool should_quit_game = false;

float current_dt = 1.0f / 60.0f;
double last_time = 0;

float live_y_max = 1.0f;
float live_y_min = -0.1f;

int num_desired_invaders = 15;
const float INVADER_RADIUS = 0.03f;

int num_shots_fired = 0;
int num_invaders_destroyed = 0;

struct Bitmap
{
    int width;
    int height;
    uint8_t *data;
    uint32_t id;
};

struct Vector2
{
    float x, y;
};

struct Vector3
{
    float x, y, z;
};

struct Vector4
{
    float x, y, z, w;
};

struct Invader
{
    Vector2 position;
    Vector2 velocity;

    Vector2 target_position;
    Bitmap *bitmap;

    float sleep_countdown = -1.0;
};

struct Particle
{
    Vector2 position;
    Vector2 velocity;

    float size;
    float lifetime;
    float drag;
    float elapsed;
    Vector4 color;
};

const int particle_max = 1000;

struct Particle_Emitter
{
    Vector2 position;
    Vector2 velocity;

    int particle_count = 0;
    Particle *particles[particle_max];
    float fadeout_period = 0.1f;
    float particles_per_second = 150.0f;

    float speed0 = 0;
    float speed1 = 0.1f;

    float size0 = 0.001f;
    float size1 = 0.005f;

    float drag0 = 0.9999f;
    float drag1 = 0.9f;

    float lifetime0 = 0.4f;
    float lifetime1 = 1.0f;

    float emitter_lifetime = -1.0f;

    float theta0 = 0;
    float theta1 = TAU;

    Vector4 color0;
    Vector4 color1;

    float elapsed = 0;
    float remainder = 0;

    bool producing = true;
};

struct Bullet
{
    Vector2 position;
    Vector2 velocity;
    Vector4 color;
    Particle_Emitter *emitter;
};

int bullet_count = 0;
const int bullet_max = 1000;
Bullet *bullets[bullet_max];

int live_invader_count = 0;
const int live_invader_max = 100;
Invader *live_invaders[live_invader_max];

int live_emitter_count = 0;
const int live_emitter_max = 1000;
Particle_Emitter *live_emitters[live_emitter_max];

const int invader_bitmap_count = 4;
Bitmap *invader_bitmaps[invader_bitmap_count];
Bitmap *ship_bitmap;
Bitmap *bullet_bitmap;
Bitmap *contrail_bitmap;

Vector2 ship_position;

uint32_t RANDRANGE = 0x10000000;
int32_t random_state = 0xbeefface;

uint32_t random_get()
{
    int32_t x = random_state;

    int32_t hi = x / 1277773;
    int32_t lo = x % 1277773;

    uint32_t t = 16807 * lo - 2836 * hi;
    if (t <= 0)
    {
        t += 0x7fffffff;
    }

    random_state = t;
    return (uint32_t)t;
}

void random_seed(int32_t new_seed)
{
    random_state = new_seed;
}

float random_get_zero_to_one()
{
    uint32_t randint = random_get() % RANDRANGE;
    return (float)randint / (float)RANDRANGE;
}

float random_get_within_range(float min, float max)
{
    float range = (max - min);
    return min + range * random_get_zero_to_one();
}

Vector2 make_vector2(float x, float y)
{
    Vector2 v;
    v.x = x;
    v.y = y;
    return v;
}

Vector3 make_vector3(float x, float y, float z)
{
    Vector3 v;
    v.x = x;
    v.y = y;
    v.z = z;
    return v;
}

Vector4 make_vector4(float x, float y, float z, float w)
{
    Vector4 v;
    v.x = x;
    v.y = y;
    v.z = z;
    v.w = w;
    return v;
}

Vector4 lerp(Vector4 a, Vector4 b, float t)
{
    Vector4 r;
    r.x = a.x + t * (b.x - a.x);
    r.y = a.y + t * (b.y - a.y);
    r.z = a.z + t * (b.z - a.z);
    r.w = a.w + t * (b.w - a.w);
    return r;
}

void linear_move(Vector2 *position, Vector2 *velocity, float dt)
{
    position->x += velocity->x * dt;
    position->y += velocity->y * dt;
}

Particle *spawn_particle(Particle_Emitter *emitter)
{
    Particle *p = new Particle;
    p->position = emitter->position;
    p->velocity = emitter->velocity;

    p->size = random_get_within_range(emitter->size0, emitter->size1);
    p->drag = random_get_within_range(emitter->drag0, emitter->drag1);
    p->lifetime = random_get_within_range(emitter->lifetime0, emitter->lifetime1);
    p->elapsed = 0;

    float color_t = random_get_within_range(0, 1);
    p->color = lerp(emitter->color0, emitter->color1, color_t);

    float speed = random_get_within_range(emitter->speed0, emitter->speed1);
    float theta = random_get_within_range(emitter->theta0, emitter->theta1);

    float ct = cosf(theta);
    float st = sinf(theta);

    Vector2 v_rel;
    v_rel.x = speed * ct;
    v_rel.y = speed * st;
    p->velocity.x += v_rel.x;
    p->velocity.y += v_rel.y;

    assert(emitter->particle_count < particle_max);
    emitter->particles[emitter->particle_count++] = p;

    return p;
}

void sim_particle(Particle *p, float dt)
{
    linear_move(&p->position, &p->velocity, dt);

    float drag = p->drag;

    p->velocity.x *= drag;
    p->velocity.y *= drag;

    p->elapsed += dt;
}

void update_emitter(Particle_Emitter *emitter)
{
    float dt = current_dt;
    int i = 0;
    while (i < emitter->particle_count)
    {
        Particle *p = emitter->particles[i];
        sim_particle(p, dt);

        if (p->elapsed > p->lifetime)
        {
            emitter->particles[i] = emitter->particles[--emitter->particle_count];
            delete p;
        }
        else
        {
            i++;
        }
    }

    float dt_per_particle = 1.0f / emitter->particles_per_second;

    emitter->elapsed += dt;
    emitter->remainder += dt;

    if (emitter->emitter_lifetime >= 0)
    {
        emitter->emitter_lifetime -= dt;
        if (emitter->emitter_lifetime < 0)
        {
            emitter->producing = false;
        }
    }

    if (emitter->producing)
    {
        while (emitter->remainder > dt_per_particle)
        {
            emitter->remainder -= dt_per_particle;
            Particle *p = spawn_particle(emitter);
            sim_particle(p, emitter->remainder);
        }
    }
    else
    {
        if (emitter->particle_count == 0)
        {
            for (int i = 0; i < live_emitter_count; i++)
            {
                if (emitter == live_emitters[i])
                {
                    live_emitters[i] = live_emitters[--live_emitter_count];
                    delete emitter;
                    break;
                }
            }
        }
    }
}

void init_target(Invader *invader)
{
    float b = 0.05f;
    invader->target_position.x = random_get_within_range(b, 1.0f - b);
    invader->target_position.y = random_get_within_range(0.2f, 0.7f);
}

void init_invader(Invader *invader)
{
    init_target(invader);

    const float INITIAL_Y = 0.8f;
    const float invader_speed = 0.01f;
    invader->position.x = invader->target_position.x;
    invader->position.y = INITIAL_Y;
    invader->velocity.x = 0;
    invader->velocity.y = 0;
}

void add_invader()
{
    Invader *invader = new Invader;

    int which = random_get() % invader_bitmap_count;
    invader->bitmap = invader_bitmaps[which];

    init_invader(invader);
    assert(live_invader_count < live_invader_max);
    live_invaders[live_invader_count++] = invader;
}

float ilength(float x, float y)
{
    float length_sq = x * x + y * y;
    return 1.0f / sqrtf(length_sq);
}

float distance(Vector2 a, Vector2 b)
{
    float dx = b.x - a.x;
    float dy = b.y - a.y;
    return sqrtf(dx*dx + dy*dy);
}

void destroy_invader(Invader *invader)
{
    {
        Particle_Emitter *emitter = new Particle_Emitter;
        assert(live_emitter_count < live_emitter_max);
        live_emitters[live_emitter_count++] = emitter;

        emitter->velocity.x = 0;
        emitter->velocity.y = 0;

        emitter->size0 = 0.01f;
        emitter->size1 = 0.04f;

        emitter->color0 = make_vector4(1, 1, 1, 1);
        emitter->color1 = make_vector4(1, 0.7f, 0.1f, 1);

        emitter->fadeout_period = 0.3f;
        emitter->emitter_lifetime = 0.3f;

        emitter->position = invader->position;
    }
    delete invader;
}

bool test_against_invaders(Bullet *bullet)
{
    for (int i = 0; i < live_invader_count; i++)
    {
        if (distance(bullet->position, live_invaders[i]->position) < INVADER_RADIUS)
        {
            destroy_invader(live_invaders[i]);
            live_invaders[i] = live_invaders[--live_invader_count];
            return true;
        }
    }
    return false;
}

bool simulate_bullet(Bullet *bullet)
{
    linear_move(&bullet->position, &bullet->velocity, current_dt);

    if (bullet->position.y > live_y_max) return true;
    if (bullet->position.y < live_y_min) return true;

    bullet->emitter->position = bullet->position;
    bullet->emitter->velocity = bullet->velocity;
    
    if (test_against_invaders(bullet)) return true;

    return false;
}

void simulate_bullets()
{
    int i = 0;
    while (i < bullet_count)
    {
        Bullet *bullet = bullets[i];
        bool done = simulate_bullet(bullet);

        if (done)
        {
            bullets[i] = bullets[--bullet_count];
            bullet->emitter->producing = false;
            delete bullet;
        }
        else
        {
            i++;
        }
    }
}

void simulate_invader(Invader *invader)
{
    if (invader->sleep_countdown < 0)
    {
        float speed = 0.3f;
        float delta = speed * current_dt;

        float dx = invader->target_position.x - invader->position.x;
        float dy = invader->target_position.y - invader->position.y;

        float denom = ilength(dx, dy);
        dx *= denom;
        dy *= denom;

        invader->position.x += dx * delta;
        invader->position.y += dy * delta;

        if (distance(invader->target_position, invader->position) < 0.005f)
        {
            invader->sleep_countdown = random_get_within_range(0.1f, 1.5f);
        }
    }
    else
    {
        invader->sleep_countdown -= current_dt;
        if (invader->sleep_countdown < 0)
        {
            init_target(invader);
        }
    }
}

void simulate_invaders()
{
    // sleep_countdown
    for (int i = 0; i < live_invader_count; i++)
    {
        simulate_invader(live_invaders[i]);
    }
}

Bullet *fire_bullet()
{
    Bullet *bullet = new Bullet;

    bullet->position = ship_position;

    bullet->velocity.x = 0;
    bullet->velocity.y = 0.4f;

    bullet->emitter = new Particle_Emitter;
    bullet->emitter->theta0 = TAU * 0.6f;
    bullet->emitter->theta1 = TAU * 0.9f;
    bullet->emitter->drag0 = 0.9f;
    bullet->emitter->drag1 = 0.97f;

    assert(live_emitter_count < live_emitter_max);
    live_emitters[live_emitter_count++] = bullet->emitter;

    float k0 = 1.0f;
    float k1 = 0.1f;

    bullet->emitter->color0 = make_vector4(k0, k0, k0, 1);
    bullet->emitter->color1 = make_vector4(k1, k1, k1, 1);

    assert(bullet_count < bullet_max);
    bullets[bullet_count++] = bullet;

    return bullet;
}

void do_fire_bullets()
{
    Bullet *left = fire_bullet();
    Bullet *right = fire_bullet();

    float offset = 0.023f;

    left->position.x -= offset;
    right->position.x += offset;

    num_shots_fired += 1;
}

void invaders_simulate()
{
    double now = get_time();

    double delta = now - last_time;
    current_dt = (float)delta;

    last_time = now;

    while (1)
    {
        if (live_invader_count < num_desired_invaders)
        {
            add_invader();
        }

        Event event;
        bool received = get_next_event(&event);
        if (!received) break;
        
        if (event.type == EVENT_TYPE_QUIT)
        {
            should_quit_game = true;
            return;
        }

        if (event.type == EVENT_TYPE_KEYBOARD)
        {
            if (event.key_code == KEY_ARROW_LEFT)
            {
                key_left = event.key_pressed;
            }
            if (event.key_code == KEY_ARROW_RIGHT)
            {
                key_right = event.key_pressed;
            }
            if (event.key_code == KEY_ARROW_DOWN)
            {
                key_down = event.key_pressed;
            }
            if (event.key_code == KEY_ARROW_UP)
            {
                key_up = event.key_pressed;
            }
            if (event.key_code == KEY_SHIFT)
            {
                if (event.key_pressed)
                {
                    do_fire_bullets();
                }
            }
            if (event.key_code == KEY_ESCAPE)
            {
                if (event.key_pressed)
                {
                    should_quit_game = true;
                }
            }
        }
    }

    float dmove = 0.3f * current_dt;
    float x0 = 0.01f;
    float x1 = 0.99f;
    float y0 = 0.02f;
    float y1 = 0.15f;

    if (key_left)
    {
        ship_position.x -= dmove;
    }
    if (key_right)
    {
        ship_position.x += dmove;
    }
    if (key_down)
    {
        ship_position.y -= dmove;
    }
    if (key_up)
    {
        ship_position.y += dmove;
    }

    if (ship_position.x < x0)
    {
        ship_position.x = x0;
    }
    if (ship_position.x > x1)
    {
        ship_position.x = x1;
    }
    if (ship_position.y < y0)
    {
        ship_position.y = y0;
    }
    if (ship_position.y > y1)
    {
        ship_position.y = y1;
    }

    simulate_bullets();
    simulate_invaders();

    for (int i = 0; i < live_emitter_count; i++)
    {
        update_emitter(live_emitters[i]);
    }
}

void init_gl_for_bitmap(Bitmap *bitmap)
{
    glGenTextures(1, &bitmap->id);
    glBindTexture(GL_TEXTURE_2D, bitmap->id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, bitmap->width, bitmap->height, 0, GL_RGBA, GL_UNSIGNED_BYTE, bitmap->data);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
}

Bitmap *load_bitmap(const char *filename)
{
    int width = 0;
    int height = 0;

    uint8_t *data = stbi_load(filename, &width, &height, NULL, 0);
    if (!data)
    {
        return NULL;
    }

    Bitmap *result = new Bitmap;
    result->width = width;
    result->height = height;
    result->data = data;

    init_gl_for_bitmap(result);

    return result;
}

void init_textures()
{
    ship_bitmap = load_bitmap("ship.png");
    bullet_bitmap = load_bitmap("bullet.png");
    contrail_bitmap = load_bitmap("contrail.png");

    invader_bitmaps[0] = load_bitmap("bug1.png");
    invader_bitmaps[1] = load_bitmap("bug2.png");
    invader_bitmaps[2] = load_bitmap("bug3.png");
    invader_bitmaps[3] = load_bitmap("bug4.png");
}

void draw_gradient()
{
    glDisable(GL_TEXTURE_2D);

    Vector2 p0 = make_vector2(0, 0);
    Vector2 p1 = make_vector2(1, 0);
    Vector2 p2 = make_vector2(1, 1);
    Vector2 p3 = make_vector2(0, 1);

    float r0 = 0.0f;
    float g0 = 89.0f / 255.0f;
    float b0 = 131.0f / 255.0f;

    float r1 = 40.0f / 255.0f;
    float g1 = 49.0f / 255.0f;
    float b1 = 95.0f / 255.0f;

    float k0 = 0.7f;
    float k1 = 0.3f;

    r0 *= k0;
    g0 *= k0;
    b0 *= k0;

    r1 *= k1;
    g1 *= k1;
    b1 *= k1;

    float z = 0;

    glBegin(GL_TRIANGLES);

    glColor3f(r0, g0, b0);
    glVertex3f(p0.x, p0.y, z);
    glColor3f(r0, g0, b0);
    glVertex3f(p1.x, p1.y, z);
    glColor3f(r1, g1, b1);
    glVertex3f(p2.x, p2.y, z);

    glColor3f(r0, g0, b0);
    glVertex3f(p0.x, p0.y, z);
    glColor3f(r1, g1, b1);
    glVertex3f(p2.x, p2.y, z);
    glColor3f(r1, g1, b1);
    glVertex3f(p3.x, p3.y, z);

    glEnd();

    glEnable(GL_TEXTURE_2D);
}

void draw_quad(Vector2 p0, Vector2 p1, Vector2 p2, Vector2 p3)
{
    float z = 0;

    glTexCoord2f(0, 1);
    glVertex3f(p0.x, p0.y, z);
    glTexCoord2f(1, 1);
    glVertex3f(p1.x, p1.y, z);
    glTexCoord2f(1, 0);
    glVertex3f(p2.x, p2.y, z);

    glTexCoord2f(0, 1);
    glVertex3f(p0.x, p0.y, z);
    glTexCoord2f(1, 0);
    glVertex3f(p2.x, p2.y, z);
    glTexCoord2f(0, 0);
    glVertex3f(p3.x, p3.y, z);
}

Vector2 moved_vector2(float x, float y, float ox, float oy, float scale, Vector2 position)
{
    x += ox;
    y += oy;

    x *= scale;
    y *= scale;

    x += position.x;
    y += position.y;

    Vector2 result;
    result.x = x;
    result.y = y;

    return result;
}

void draw_quad_centered_at(Vector2 position, float radius)
{
    Vector2 p0 = position;
    Vector2 p1 = position;
    Vector2 p2 = position;
    Vector2 p3 = position;
    float b = radius;

    p0.x -= b;
    p0.y -= b;

    p1.x += b;
    p1.y -= b;

    p2.x += b;
    p2.y += b;

    p3.x -= b;
    p3.y += b;

    draw_quad(p0, p1, p2, p3);
}

void draw_emitter(Particle_Emitter *emitter)
{
    glBindTexture(GL_TEXTURE_2D, contrail_bitmap->id);

    glBegin(GL_TRIANGLES);

    for (int i = 0; i < emitter->particle_count; i++)
    {
        Particle *p = emitter->particles[i];

        float alpha = 1.0f;

        float tail_time = p->lifetime - p->elapsed;
        if (tail_time < emitter->fadeout_period)
        {
            float t = tail_time / emitter->fadeout_period;
            if (t < 0) t = 0;
            if (t > 1) t = 1;
            alpha = t;
        }

        Vector4 c = p->color;
        glColor4f(c.x, c.y, c.z, c.w * alpha);
        draw_quad_centered_at(p->position, p->size);
    }

    glEnd();
}

void draw_bullet(Bullet *bullet)
{
    Vector2 position = bullet->position;
    float bullet_size = 0.02f;

    glBindTexture(GL_TEXTURE_2D, bullet_bitmap->id);
    glBegin(GL_TRIANGLES);
    glColor4f(1, 1, 1, 1);
    draw_quad_centered_at(position, bullet_size);
    glEnd();
}

void draw_ship()
{
    float ship_size = 0.04f;
    glBindTexture(GL_TEXTURE_2D, ship_bitmap->id);
    glBegin(GL_TRIANGLES);
    glColor4f(1, 1, 1, 1);
    draw_quad_centered_at(ship_position, ship_size);
    glEnd();
}

void draw_invader(Invader *invader)
{
    float invader_size = 0.03f;
    glBindTexture(GL_TEXTURE_2D, invader->bitmap->id);
    glBegin(GL_TRIANGLES);
    glColor4f(1, 1, 1, 1);
    draw_quad_centered_at(invader->position, invader_size);
    glEnd();
}

int invaders()
{
    last_time = get_time();

    int width = 800;
    int height = 600;

    create_window(width, height);
    init_textures();

    for (int i = 0; i < num_desired_invaders; i++)
    {
        add_invader();
    }

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    double aspect = (double)height / (double)width;
    glOrtho(0, 1, 0, aspect, -1, 1);

    ship_position.x = 0.5f;
    ship_position.y = 0.1f;

    while (1)
    {
        if (should_quit_game)
        {
            return num_invaders_destroyed;
        }

        float k = 0.05f;
        window_clear(k, k, k, 1);

        draw_gradient();

        invaders_simulate();

        draw_ship();

        for (int i = 0; i < bullet_count; i++)
        {
            draw_bullet(bullets[i]);
        }
        for (int i = 0; i < live_invader_count; i++)
        {
            draw_invader(live_invaders[i]);
        }
        for (int i = 0; i < live_emitter_count; i++)
        {
            draw_emitter(live_emitters[i]);
        }

        swap_buffers();

        do_sleep(5);

        update_window_events();
    }
}
