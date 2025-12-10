#pragma once
#include <math.h>

struct Vector3 {
    float x, y, z;
    
    Vector3() : x(0), y(0), z(0) {}
    Vector3(float x, float y, float z) : x(x), y(y), z(z) {}
    
    float Distance(const Vector3& other) const {
        float dx = x - other.x;
        float dy = y - other.y;
        float dz = z - other.z;
        return sqrtf(dx * dx + dy * dy + dz * dz);
    }
    
    Vector3 operator-(const Vector3& other) const {
        return Vector3(x - other.x, y - other.y, z - other.z);
    }
    
    Vector3 operator+(const Vector3& other) const {
        return Vector3(x + other.x, y + other.y, z + other.z);
    }

    Vector3 operator*(float scalar) const {
        return Vector3(x * scalar, y * scalar, z * scalar);
    }

    Vector3 operator/(float scalar) const {
        return Vector3(x / scalar, y / scalar, z / scalar);
    }

    float Length() const {
        return sqrtf(x * x + y * y + z * z);
    }

    float Dot(const Vector3& other) const {
        return x * other.x + y * other.y + z * other.z;
    }

    bool IsZero() const {
        return x == 0 && y == 0 && z == 0;
    }
};

struct Vector2 {
    float x, y;
    
    Vector2() : x(0), y(0) {}
    Vector2(float x, float y) : x(x), y(y) {}
    
    bool IsValid(float width, float height) const {
        return x >= 0 && x <= width && y >= 0 && y <= height;
    }
};

struct Matrix {
    float matrix[16];
    
    float& operator[](int index) {
        return matrix[index];
    }
    
    const float& operator[](int index) const {
        return matrix[index];
    }
};

inline Vector2 WorldToScreen(const Vector3& pos, const Matrix& matrix, float width, float height) {
    Vector2 screen;
    
    float w = matrix[12] * pos.x + matrix[13] * pos.y + matrix[14] * pos.z + matrix[15];
    
    if (w < 0.01f) {
        return Vector2(-1, -1);
    }
    
    float x = matrix[0] * pos.x + matrix[1] * pos.y + matrix[2] * pos.z + matrix[3];
    float y = matrix[4] * pos.x + matrix[5] * pos.y + matrix[6] * pos.z + matrix[7];
    
    screen.x = (width / 2.0f) + (width / 2.0f) * x / w;
    screen.y = (height / 2.0f) - (height / 2.0f) * y / w;
    
    return screen;
}
