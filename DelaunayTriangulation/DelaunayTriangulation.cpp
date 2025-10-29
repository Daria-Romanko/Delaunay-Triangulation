﻿#include <SFML/Graphics.hpp>
#include "imgui.h"
#include "imgui-SFML.h"
#include <random>
#include <vector>
#include <algorithm>
#include <cmath>
#include <stack>
#include <iostream>
#include <list>
#include <set>

struct Point {
    double x, y;
    Point(double x = 0, double  y = 0) : x(x), y(y) {}

    bool operator==(const Point& other) const {
        return std::abs(x - other.x) < 1e-9 && std::abs(y - other.y) < 1e-9;
    }
};

struct Edge {
    Point p1, p2;
    Edge(Point p1, Point p2) : p1(p1), p2(p2) {}

    bool operator==(const Edge& other) const {
        return (p1 == other.p1 && p2 == other.p2) || (p1 == other.p2 && p2 == other.p1);
    }

    bool operator<(const Edge& other) const {
        // Сортируем точки в ребре для consistent сравнения
        Point min1 = (p1.x < p2.x || (std::abs(p1.x - p2.x) < 1e-9 && p1.y < p2.y)) ? p1 : p2;
        Point max1 = (min1 == p1) ? p2 : p1;

        Point min2 = (other.p1.x < other.p2.x || (std::abs(other.p1.x - other.p2.x) < 1e-9 && other.p1.y < other.p2.y)) ? other.p1 : other.p2;
        Point max2 = (min2 == other.p1) ? other.p2 : other.p1;

        // Сравниваем сначала по минимальной точке, потом по максимальной
        if (min1.x != min2.x) return min1.x < min2.x;
        if (min1.y != min2.y) return min1.y < min2.y;
        if (max1.x != max2.x) return max1.x < max2.x;
        return max1.y < max2.y;
    }
};

struct Triangle {
    Point p1, p2, p3;
    Triangle(Point p1, Point p2, Point p3) : p1(p1), p2(p2), p3(p3) {}
    bool operator==(const Triangle& other) const {
        return (p1 == other.p1 || p1 == other.p2 || p1 == other.p3) &&
            (p2 == other.p1 || p2 == other.p2 || p2 == other.p3) &&
            (p3 == other.p1 || p3 == other.p2 || p3 == other.p3);
    }
};

int pointSideOfEdge(const Point& a, const Point& b, const Point& c){
    double det = (b.y - a.y) * (c.x - a.x) - (b.x - a.x) * (c.y - a.y); 
    if (std::abs(det) < 1e-9) {
        return 0;
    }
    else if (det > 0) {
        return 1; //слева
    }
    else {
        return -1; // справа
    }
}

class DelaunayTriangulation {
private:
    std::vector<Point> points;
    std::vector<Triangle> triangles;
    std::list<Edge> activeEdges;
    std::list<Edge> deadEdges;

    Edge findFirstEdge() {
        if (points.size() < 3) return Edge(Point(0, 0), Point(1, 1));

        //найти самую левую нижнюю точку
        int startInd = 0;
        for (int i = 1; i < points.size(); i++) {
            if (points[i].x < points[startInd].x || (std::abs(points[i].x - points[startInd].x) < 1e-9 && points[i].y < points[startInd].y)) {
                startInd = i;
            }
        }
        
        int nextInd = (startInd == 0) ? 1 : 0;
        for (int i = 0; i < points.size(); ++i) {
            if (i == startInd || i == nextInd) continue;

            int det = pointSideOfEdge(points[startInd], points[nextInd], points[i]);
            if (det == -1) {
                nextInd = i;
            }
            else if (det == 0) {
                double d1 = std::sqrt((points[nextInd].x - points[startInd].x) * (points[nextInd].x - points[startInd].x) + (points[nextInd].y - points[startInd].y) * (points[nextInd].y - points[startInd].y));
                double d2 = std::sqrt((points[i].x - points[startInd].x) * (points[i].x - points[startInd].x) + (points[i].y - points[startInd].y) * (points[i].y - points[startInd].y));
                if (d2 > d1) nextInd = i;
            }
        }

        return Edge(points[startInd], points[nextInd]);
    }

    Point calculateCircleCenter(const Point& a, const Point& b, const Point& c) {
        double d = 2 * (a.x * (b.y - c.y) + b.x * (c.y - a.y) + c.x * (a.y - b.y));
        if (std::abs(d) < 1e-9) return Point(0, 0);

        double ux = ((a.x * a.x + a.y * a.y) * (b.y - c.y) +
            (b.x * b.x + b.y * b.y) * (c.y - a.y) +
            (c.x * c.x + c.y * c.y) * (a.y - b.y)) / d;

        double uy = ((a.x * a.x + a.y * a.y) * (c.x - b.x) +
            (b.x * b.x + b.y * b.y) * (a.x - c.x) +
            (c.x * c.x + c.y * c.y) * (b.x - a.x)) / d;

        return Point(ux, uy);
    }

    double calculateDistance(const Point& c, const Edge& e) {
        Point mid;
        mid.x = e.p1.x + (e.p2.x - e.p1.x) * 0.5;
        mid.y = e.p1.y + (e.p2.y - e.p1.y) * 0.5;

        Point vec;
        vec.x = c.x - mid.x;
        vec.y = c.y - mid.y;

        Point dir;
        dir.x = e.p2.x - e.p1.x;
        dir.y = e.p2.y - e.p1.y;

        Point normal;
        normal.x = -dir.y;
        normal.y = dir.x;

        return std::sqrt(vec.x * vec.x + vec.y * vec.y) * ((vec.x * normal.x + vec.y * normal.y >= 0) ? 1.0 : -1.0);
    }

    Point findRightConjugatePoint(const Edge& e) {
        Point p;
        double minDist = std::numeric_limits<double>::max();
        bool found = false;

        for (int i = 0; i < points.size(); ++i) {
            if (points[i] == e.p1 || points[i] == e.p2) continue;

            if (pointSideOfEdge(e.p1, e.p2, points[i]) == -1) {
                Point c = calculateCircleCenter(e.p1, e.p2, points[i]);

                double dist = calculateDistance(c, e);

                if (dist < minDist) {
                    minDist = dist;
                    p = points[i];
                    found = true;
                }
            }
        }

        if (!found) {
            return Point(std::numeric_limits<double>::max(), std::numeric_limits<double>::max());
        }

        return p;

    }

public:
    const std::vector<Point>& getPoints() const { return points; }
    const std::vector<Triangle>& getTriangles() const { return triangles; }

    void clear() {
        points.clear();
        triangles.clear();
    }

    void addPoint(double x, double y) { points.push_back(Point(x, y)); }

    void generateRandPoints(int count, double w, double h) {
        clear();
        
        std::uniform_real_distribution<double> dist_x(30.0, w - 30.0);
        std::uniform_real_distribution<double> dist_y(30.0, h - 30.0);

        std::default_random_engine re;

        for (int i = 0; i < count; ++i) {
            points.push_back(Point(dist_x(re), dist_y(re)));
        }
    }

    void triangulate() {
        triangles.clear();
        activeEdges.clear();
        deadEdges.clear();
        if (points.size() < 3) return;

        Edge firstEdge = findFirstEdge();

        bool hasRightPoints = false;
        for (const auto& point : points) {
            if (point == firstEdge.p1 || point == firstEdge.p2) continue;
            if (pointSideOfEdge(firstEdge.p1, firstEdge.p2, point) == -1) {
                hasRightPoints = true;
                break;
            }
        }

        if (!hasRightPoints) {
            firstEdge = Edge(firstEdge.p2, firstEdge.p1);
        }


        activeEdges.push_back(firstEdge);

        while (!activeEdges.empty()) {
            Edge curEdge = activeEdges.front();
            activeEdges.pop_front();

            //найти правую сопряженную точку
            Point conjugatePoint = findRightConjugatePoint(curEdge);

            //не нашли точку справа - ребро граничное
            if (conjugatePoint.x == std::numeric_limits<double>::max()) {
                deadEdges.push_back(curEdge);
                continue;
            }

            //новый треугольник
            Triangle newTriangle(curEdge.p1, curEdge.p2, conjugatePoint);
            triangles.push_back(newTriangle);

            Edge newEdge1(curEdge.p1, conjugatePoint);
            Edge newEdge2(conjugatePoint, curEdge.p2);

            if (std::find(activeEdges.begin(), activeEdges.end(), newEdge1) != activeEdges.end()) {
                activeEdges.remove(newEdge1);
                deadEdges.push_back(newEdge1);
            }
            else {
                if (pointSideOfEdge(newEdge1.p1, newEdge1.p2, curEdge.p2) == -1) {
                    newEdge1 = Edge(newEdge1.p2, newEdge1.p1);
                }
                if (std::find(deadEdges.begin(), deadEdges.end(), newEdge1) == deadEdges.end()) {
                    activeEdges.push_back(newEdge1);
                }
            }

            if (std::find(activeEdges.begin(), activeEdges.end(), newEdge2) != activeEdges.end()) {
                activeEdges.remove(newEdge2);
                deadEdges.push_back(newEdge2);
            }
            else {
                if (pointSideOfEdge(newEdge2.p1, newEdge2.p2, curEdge.p1) == -1) {
                    newEdge2 = Edge(newEdge2.p2, newEdge2.p1);
                }
                if (std::find(deadEdges.begin(), deadEdges.end(), newEdge2) == deadEdges.end()) {
                    activeEdges.push_back(newEdge2);
                }
            }

            deadEdges.push_back(curEdge);
        }
    }
};

int main()
{
    const int wWidth = 1200;
    const int wHeight = 800;

    sf::RenderWindow window(sf::VideoMode(wWidth, wHeight), "Delaunay Triangulation");
    window.setFramerateLimit(60);

    ImGui::SFML::Init(window);

    DelaunayTriangulation  dt;

    bool drawPoints = true;
    bool drawTriangles = true;
    bool drawCircles = false;
    float circleScale = 1.0f;

    int pointCount = 50;
    char pointCountInput[10] = "50";

    sf::Clock deltaClock;

    while (window.isOpen()) {
        sf::Event event;
        while (window.pollEvent(event)) {
            ImGui::SFML::ProcessEvent(window, event);

            if (event.type == sf::Event::Closed) {
                window.close();
            }

            if (event.type == sf::Event::MouseButtonPressed) {
                if (event.mouseButton.button == sf::Mouse::Left) {
                    sf::Vector2i mousePos = sf::Mouse::getPosition(window);
                    dt.addPoint(mousePos.x, mousePos.y);
                }
            }
        }
        
        ImGui::SFML::Update(window, deltaClock.restart());

        ImGui::Begin("Delaunay Triangulation Control");

        ImGui::InputText("Points", pointCountInput, 10);

        try {
            pointCount = std::stoi(pointCountInput);
            pointCount = std::max(1, std::min(pointCount, 500));
        }
        catch (...) {
            pointCount = 50;
        }

        if (ImGui::Button("Generate Random Points")) {
            dt.generateRandPoints(pointCount, wWidth, wHeight);
        }

        ImGui::SameLine();
        if (ImGui::Button("Clear")) {
            dt.clear();
        }

        ImGui::Separator();
        if (ImGui::Button("Triangulate")) {
            dt.triangulate();
        }

        ImGui::Separator();
        ImGui::Text("Visualization:");

        ImGui::Checkbox("Show Points", &drawPoints);
        ImGui::Checkbox("Show Triangles", &drawTriangles);
        ImGui::Checkbox("Show Circles", &drawCircles);
        ImGui::SliderFloat("Circle Scale", &circleScale, 0.1f, 3.0f, "%.2f");

        ImGui::End();

        window.clear(sf::Color(40, 44, 52));

        if (drawTriangles) {
            const auto& triangles = dt.getTriangles();
            std::vector<Edge> uniqueEdges;

            for (const auto& triangle : triangles) {
                Edge edges[3] = {
                    Edge(triangle.p1, triangle.p2),
                    Edge(triangle.p2, triangle.p3),
                    Edge(triangle.p3, triangle.p1)
                };

                for (const auto& edge : edges) {
                    bool found = false;
                    for (const auto& uniqueEdge : uniqueEdges) {
                        if (edge == uniqueEdge) {
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        uniqueEdges.push_back(edge);
                    }
                }
            }

            for (const auto& edge : uniqueEdges) {
                sf::Vertex line[] = {
                    sf::Vertex(sf::Vector2f(static_cast<float>(edge.p1.x),
                                           static_cast<float>(edge.p1.y))),
                    sf::Vertex(sf::Vector2f(static_cast<float>(edge.p2.x),
                                           static_cast<float>(edge.p2.y)))
                };
                line[0].color = sf::Color(200, 200, 200);
                line[1].color = sf::Color(200, 200, 200);
                window.draw(line, 2, sf::Lines);
            }
        }
        if (drawCircles) {
        const auto& triangles = dt.getTriangles();
        for (const auto& t : triangles) {
            // Вычисляем центр и радиус окружности
            double d = 2 * (t.p1.x * (t.p2.y - t.p3.y) +
                            t.p2.x * (t.p3.y - t.p1.y) +
                            t.p3.x * (t.p1.y - t.p2.y));
            if (std::abs(d) < 1e-9) continue; // треугольник вырожден

            double ux = ((t.p1.x * t.p1.x + t.p1.y * t.p1.y) * (t.p2.y - t.p3.y) +
                         (t.p2.x * t.p2.x + t.p2.y * t.p2.y) * (t.p3.y - t.p1.y) +
                         (t.p3.x * t.p3.x + t.p3.y * t.p3.y) * (t.p1.y - t.p2.y)) / d;

            double uy = ((t.p1.x * t.p1.x + t.p1.y * t.p1.y) * (t.p3.x - t.p2.x) +
                         (t.p2.x * t.p2.x + t.p2.y * t.p2.y) * (t.p1.x - t.p3.x) +
                         (t.p3.x * t.p3.x + t.p3.y * t.p3.y) * (t.p2.x - t.p1.x)) / d;

            Point center(ux, uy);
            double r = std::sqrt((center.x - t.p1.x) * (center.x - t.p1.x) +
                                 (center.y - t.p1.y) * (center.y - t.p1.y));

            sf::CircleShape circle;
            circle.setRadius(static_cast<float>(r * circleScale));
            circle.setOrigin(circle.getRadius(), circle.getRadius());
            circle.setPosition(center.x, center.y);
            circle.setFillColor(sf::Color::Transparent);
            circle.setOutlineThickness(1);
            circle.setOutlineColor(sf::Color(255, 100, 100, 80));
            window.draw(circle);
        }
    }
        if (drawPoints) {
            const auto& points = dt.getPoints();
            for (const auto& point : points) {
                sf::CircleShape circle(3);
                circle.setPosition(point.x - 3, point.y - 3);
                circle.setFillColor(sf::Color::Magenta);
                window.draw(circle);
            }
        }

        ImGui::SFML::Render(window);
        window.display();
    }

    ImGui::SFML::Shutdown();
    return 0;
}

