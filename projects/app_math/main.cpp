#include <iostream>

struct Vec2
{
    float x;
    float y;
};

Vec2 Add(Vec2 a, Vec2 b)
{
    return { a.x + b.x, a.y + b.y };
}

int main()
{
    Vec2 a{1.0f, 2.0f};
    Vec2 b{3.0f, 4.0f};

    Vec2 c = Add(a, b);

    std::cout << "Result: (" << c.x << ", " << c.y << ")\n";
}