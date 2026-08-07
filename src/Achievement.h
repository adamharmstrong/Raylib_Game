#pragma once

#include <deque>
#include <string>
#include <vector>

struct Achievement {
    std::string id;
    std::string title;
    std::string description;
    bool unlocked{false};
};

class AchievementSystem {
public:
    void Initialize(const std::string& definitionsPath, const std::string& progressPath);
    bool Unlock(const std::string& id);
    void ResetProgress();
    void Update(float dt);
    void Draw(int screenWidth, int screenHeight) const;

    const std::vector<Achievement>& GetAchievements() const;

private:
    void LoadDefinitions(const std::string& path);
    void LoadProgress();
    void SaveProgress() const;
    int FindAchievement(const std::string& id) const;
    void StartNextToast();

    std::vector<Achievement> achievements;
    std::deque<int> queuedToasts;
    std::string progressPath;
    int activeToast{-1};
    float toastTimer{0.0f};
};
