#include "Achievement.h"

#include "raylib.h"

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_set>

namespace {
    constexpr float ToastDuration = 5.4f;

    std::string Trim(std::string value) {
        const size_t first = value.find_first_not_of(" \t\r\n");
        if (first == std::string::npos) return {};
        const size_t last = value.find_last_not_of(" \t\r\n");
        return value.substr(first, last - first + 1);
    }

    float SmoothStep(float value) {
        value = std::clamp(value, 0.0f, 1.0f);
        return value * value * (3.0f - 2.0f * value);
    }

    void DrawTrophy(Vector2 center, Color color) {
        DrawRectangleRounded({center.x - 15.0f, center.y - 20.0f, 30.0f, 30.0f}, 0.28f, 6, color);
        DrawRectangleRoundedLinesEx({center.x - 15.0f, center.y - 20.0f, 30.0f, 30.0f}, 0.28f, 6, 2.0f, BLACK);
        DrawLineEx({center.x, center.y + 10.0f}, {center.x, center.y + 22.0f}, 5.0f, color);
        DrawRectangleRounded({center.x - 14.0f, center.y + 20.0f, 28.0f, 7.0f}, 0.35f, 4, color);
        DrawRing({center.x - 15.0f, center.y - 7.0f}, 7.0f, 10.0f, 90.0f, 270.0f, 12, color);
        DrawRing({center.x + 15.0f, center.y - 7.0f}, 7.0f, 10.0f, -90.0f, 90.0f, 12, color);
    }
}

void AchievementSystem::Initialize(const std::string& definitionsPath, const std::string& savePath) {
    achievements.clear();
    queuedToasts.clear();
    activeToast = -1;
    toastTimer = 0.0f;
    progressPath = savePath;
    LoadDefinitions(definitionsPath);

    // Keep the first achievement available even if game data was not copied
    // beside a development executable yet.
    if (achievements.empty()) {
        achievements.push_back({
            "still_alive",
            "Still Alive",
            "Shut off Level 5's deadly neurotoxin.",
            false
        });
    }
    LoadProgress();
}

bool AchievementSystem::Unlock(const std::string& id) {
    const int index = FindAchievement(id);
    if (index < 0 || achievements[index].unlocked) {
        return false;
    }

    achievements[index].unlocked = true;
    queuedToasts.push_back(index);
    SaveProgress();
    StartNextToast();
    return true;
}

void AchievementSystem::ResetProgress() {
    for (Achievement& achievement : achievements) achievement.unlocked = false;
    queuedToasts.clear();
    activeToast = -1;
    toastTimer = 0.0f;
    SaveProgress();
}

void AchievementSystem::Update(float dt) {
    if (activeToast < 0) {
        StartNextToast();
        return;
    }

    toastTimer = std::max(0.0f, toastTimer - std::max(0.0f, dt));
    if (toastTimer <= 0.0f) {
        activeToast = -1;
        StartNextToast();
    }
}

void AchievementSystem::Draw(int screenWidth, int) const {
    if (activeToast < 0 || activeToast >= static_cast<int>(achievements.size())) return;

    const Achievement& achievement = achievements[activeToast];
    constexpr float width = 470.0f;
    constexpr float height = 112.0f;
    constexpr float margin = 22.0f;
    const float elapsed = ToastDuration - toastTimer;
    const float enter = SmoothStep(elapsed / 0.38f);
    const float leave = toastTimer < 0.45f ? SmoothStep(toastTimer / 0.45f) : 1.0f;
    const float visibility = std::min(enter, leave);
    const float x = static_cast<float>(screenWidth) - margin - width + (1.0f - visibility) * (width + margin);
    const float y = margin;
    const unsigned char alpha = static_cast<unsigned char>(230.0f * visibility);

    DrawRectangleRounded({x, y, width, height}, 0.14f, 8, Color{18, 24, 29, alpha});
    DrawRectangleRoundedLinesEx({x, y, width, height}, 0.14f, 8, 2.0f, Color{224, 165, 53, alpha});
    DrawRectangleRounded({x + 10.0f, y + 10.0f, 78.0f, height - 20.0f}, 0.18f, 6, Color{45, 53, 58, alpha});
    DrawTrophy({x + 49.0f, y + 53.0f}, Color{224, 165, 53, alpha});
    DrawText("ACHIEVEMENT UNLOCKED", static_cast<int>(x + 104.0f), static_cast<int>(y + 15.0f), 17,
        Color{224, 165, 53, alpha});
    DrawText(achievement.title.c_str(), static_cast<int>(x + 104.0f), static_cast<int>(y + 40.0f), 27,
        Color{245, 246, 238, alpha});
    DrawText(achievement.description.c_str(), static_cast<int>(x + 104.0f), static_cast<int>(y + 76.0f), 16,
        Color{196, 204, 207, alpha});
}

const std::vector<Achievement>& AchievementSystem::GetAchievements() const {
    return achievements;
}

void AchievementSystem::LoadDefinitions(const std::string& path) {
    std::ifstream file(path);
    std::string line;
    while (std::getline(file, line)) {
        line = Trim(line);
        if (line.empty() || line[0] == '#') continue;

        std::istringstream stream(line);
        std::string id;
        std::string title;
        std::string description;
        if (!std::getline(stream, id, '|') ||
            !std::getline(stream, title, '|') ||
            !std::getline(stream, description)) {
            continue;
        }

        id = Trim(id);
        title = Trim(title);
        description = Trim(description);
        if (!id.empty() && !title.empty() && FindAchievement(id) < 0) {
            achievements.push_back({id, title, description, false});
        }
    }
}

void AchievementSystem::LoadProgress() {
    std::ifstream file(progressPath);
    std::unordered_set<std::string> unlockedIds;
    std::string id;
    while (std::getline(file, id)) {
        id = Trim(id);
        if (!id.empty() && id[0] != '#') unlockedIds.insert(id);
    }

    for (Achievement& achievement : achievements) {
        achievement.unlocked = unlockedIds.find(achievement.id) != unlockedIds.end();
    }
}

void AchievementSystem::SaveProgress() const {
    const std::filesystem::path path(progressPath);
    std::error_code error;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), error);

    std::ofstream file(progressPath, std::ios::trunc);
    if (!file) return;
    file << "# Unlocked achievement ids\n";
    for (const Achievement& achievement : achievements) {
        if (achievement.unlocked) file << achievement.id << '\n';
    }
}

int AchievementSystem::FindAchievement(const std::string& id) const {
    for (int index = 0; index < static_cast<int>(achievements.size()); ++index) {
        if (achievements[index].id == id) return index;
    }
    return -1;
}

void AchievementSystem::StartNextToast() {
    if (activeToast >= 0 || queuedToasts.empty()) return;
    activeToast = queuedToasts.front();
    queuedToasts.pop_front();
    toastTimer = ToastDuration;
}
