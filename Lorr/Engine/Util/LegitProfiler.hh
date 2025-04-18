/*
Copyright 2020 Alexander Sannikov

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <imgui.h>
#include <map>
#include <sstream>

namespace legit {
namespace Colors {
    // https://flatuicolors.com/palette/defo
#define RGBA_LE(col) \
    ((((col) & 0xff000000) >> (3 * 8)) + (((col) & 0x00ff0000) >> (1 * 8)) + (((col) & 0x0000ff00) << (1 * 8)) + (((col) & 0x000000ff) << (3 * 8)))
    const static uint32_t turqoise = RGBA_LE(0x1abc9cffu);
    const static uint32_t greenSea = RGBA_LE(0x16a085ffu);

    const static uint32_t emerald = RGBA_LE(0x2ecc71ffu);
    const static uint32_t nephritis = RGBA_LE(0x27ae60ffu);

    const static uint32_t peterRiver = RGBA_LE(0x3498dbffu); // blue
    const static uint32_t belizeHole = RGBA_LE(0x2980b9ffu);

    const static uint32_t amethyst = RGBA_LE(0x9b59b6ffu);
    const static uint32_t wisteria = RGBA_LE(0x8e44adffu);

    const static uint32_t sunFlower = RGBA_LE(0xf1c40fffu);
    const static uint32_t orange = RGBA_LE(0xf39c12ffu);

    const static uint32_t carrot = RGBA_LE(0xe67e22ffu);
    const static uint32_t pumpkin = RGBA_LE(0xd35400ffu);

    const static uint32_t alizarin = RGBA_LE(0xe74c3cffu);
    const static uint32_t pomegranate = RGBA_LE(0xc0392bffu);

    const static uint32_t clouds = RGBA_LE(0xecf0f1ffu);
    const static uint32_t silver = RGBA_LE(0xbdc3c7ffu);
    const static uint32_t imguiText = RGBA_LE(0xF2F5FAFFu);

    constexpr static uint32_t colors[] = { turqoise,  greenSea, emerald, nephritis, peterRiver, belizeHole,  amethyst, wisteria,
                                           sunFlower, orange,   carrot,  pumpkin,   alizarin,   pomegranate, clouds,   silver };
} // namespace Colors

struct ProfilerTask {
    double startTime {};
    double endTime {};
    std::string name;
    uint32_t color {};
    double GetLength() {
        return endTime - startTime;
    }
};

inline glm::vec2 Vec2(ImVec2 vec) {
    return { vec.x, vec.y };
}

class ProfilerGraph {
public:
    int frameWidth;
    int frameSpacing;
    bool useColoredLegendText = true;
    float maxFrameTime = 1000.0f;

    ProfilerGraph(size_t framesCount) {
        frames.resize(framesCount);
        for (auto &frame : frames)
            frame.tasks.reserve(100);
        frameWidth = 3;
        frameSpacing = 1;
        useColoredLegendText = true;
    }

    void LoadFrameData(const legit::ProfilerTask *tasks, size_t count) {
        auto &currFrame = frames[currFrameIndex];
        currFrame.tasks.resize(0);
        for (size_t taskIndex = 0; taskIndex < count; taskIndex++) {
            if (taskIndex == 0)
                currFrame.tasks.push_back(tasks[taskIndex]);
            else {
                if (tasks[taskIndex - 1].color != tasks[taskIndex].color || tasks[taskIndex - 1].name != tasks[taskIndex].name) {
                    currFrame.tasks.push_back(tasks[taskIndex]);
                } else {
                    currFrame.tasks.back().endTime = tasks[taskIndex].endTime;
                }
            }
        }
        currFrame.taskStatsIndex.resize(currFrame.tasks.size());

        for (size_t taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++) {
            auto &task = currFrame.tasks[taskIndex];
            auto it = taskNameToStatsIndex.find(task.name);
            if (it == taskNameToStatsIndex.end()) {
                taskNameToStatsIndex[task.name] = taskStats.size();
                TaskStats taskStat {};
                taskStat.maxTime = -1.0;
                taskStats.push_back(taskStat);
            }
            currFrame.taskStatsIndex[taskIndex] = taskNameToStatsIndex[task.name];
        }
        currFrameIndex = (currFrameIndex + 1) % frames.size();

        RebuildTaskStats(currFrameIndex, 300 /*frames.size()*/);
    }

    void RenderTimings(int graphWidth, int legendWidth, int height, size_t frameIndexOffset) {
        ImDrawList *drawList = ImGui::GetWindowDrawList();
        const glm::vec2 widgetPos = Vec2(ImGui::GetCursorScreenPos());
        RenderGraph(drawList, widgetPos, glm::vec2(graphWidth, height), frameIndexOffset);
        RenderLegend(drawList, widgetPos + glm::vec2(graphWidth, 0.0f), glm::vec2(legendWidth, height), frameIndexOffset);
        ImGui::Dummy(ImVec2(float(graphWidth + legendWidth), float(height)));
    }

private:
    void RebuildTaskStats(size_t endFrame, size_t framesCount) {
        for (auto &taskStat : taskStats) {
            taskStat.maxTime = -1.0f;
            taskStat.priorityOrder = size_t(-1);
            taskStat.onScreenIndex = size_t(-1);
        }

        for (size_t frameNumber = 0; frameNumber < framesCount; frameNumber++) {
            size_t frameIndex = (endFrame - 1 - frameNumber + frames.size()) % frames.size();
            auto &frame = frames[frameIndex];
            for (size_t taskIndex = 0; taskIndex < frame.tasks.size(); taskIndex++) {
                auto &task = frame.tasks[taskIndex];
                auto &stats = taskStats[frame.taskStatsIndex[taskIndex]];
                stats.maxTime = std::max(stats.maxTime, task.endTime - task.startTime);
            }
        }
        std::vector<size_t> statPriorities;
        statPriorities.resize(taskStats.size());
        for (size_t statIndex = 0; statIndex < taskStats.size(); statIndex++)
            statPriorities[statIndex] = statIndex;

        std::sort(statPriorities.begin(), statPriorities.end(), [this](size_t left, size_t right) {
            return taskStats[left].maxTime > taskStats[right].maxTime;
        });
        for (size_t statNumber = 0; statNumber < taskStats.size(); statNumber++) {
            size_t statIndex = statPriorities[statNumber];
            taskStats[statIndex].priorityOrder = statNumber;
        }
    }

    void RenderGraph(ImDrawList *drawList, glm::vec2 graphPos, glm::vec2 graphSize, size_t frameIndexOffset) {
        glm::vec2 area_size = graphPos + graphSize;
        Rect(drawList, graphPos, area_size, 0xffffffff, false);
        float heightThreshold = 1.0f;

        ImGui::PushClipRect({ graphPos.x, graphPos.y }, { area_size.x, area_size.y }, true);

        for (size_t frameNumber = 0; frameNumber < frames.size(); frameNumber++) {
            size_t frameIndex = (currFrameIndex - frameIndexOffset - 1 - frameNumber + 2 * frames.size()) % frames.size();

            glm::vec2 framePos =
                graphPos + glm::vec2(graphSize.x - 1 - (float)frameWidth - (float)(frameWidth + frameSpacing) * (float)frameNumber, graphSize.y - 1);
            if (framePos.x < graphPos.x + 1)
                break;
            glm::vec2 taskPos = framePos + glm::vec2(0.0f, 0.0f);
            auto &frame = frames[frameIndex];
            for (const auto &task : frame.tasks) {
                float taskStartHeight = (float(task.startTime) / (1.0f / maxFrameTime)) * graphSize.y;
                float taskEndHeight = (float(task.endTime) / (1.0f / maxFrameTime)) * graphSize.y;
                // taskMaxCosts[task.name] = std::max(taskMaxCosts[task.name], task.endTime - task.startTime);
                if (abs(taskEndHeight - taskStartHeight) > heightThreshold)
                    Rect(drawList, taskPos + glm::vec2(0.0f, -taskStartHeight), taskPos + glm::vec2(frameWidth, -taskEndHeight), task.color, true);
            }
        }

        ImGui::PopClipRect();
    }
    void RenderLegend(ImDrawList *drawList, glm::vec2 legendPos, glm::vec2 legendSize, size_t frameIndexOffset) {
        float markerLeftRectMargin = 3.0f;
        float markerLeftRectWidth = 5.0f;
        float markerMidWidth = 30.0f;
        float markerRightRectWidth = 10.0f;
        float markerRigthRectMargin = 3.0f;
        float markerRightRectHeight = 10.0f;
        float markerRightRectSpacing = 4.0f;
        float nameOffset = 50.0f;
        glm::vec2 textMargin = glm::vec2(5.0f, -3.0f);

        ImGui::PushClipRect({ legendPos.x, legendPos.y }, { FLT_MAX, legendPos.y + legendSize.y }, true);

        auto &currFrame = frames[(currFrameIndex - frameIndexOffset - 1 + 2 * frames.size()) % frames.size()];
        size_t maxTasksCount = size_t(legendSize.y / (markerRightRectHeight + markerRightRectSpacing));

        for (auto &taskStat : taskStats) {
            taskStat.onScreenIndex = size_t(-1);
        }

        size_t tasksToShow = std::min<size_t>(taskStats.size(), maxTasksCount);
        size_t tasksShownCount = 0;
        for (size_t taskIndex = 0; taskIndex < currFrame.tasks.size(); taskIndex++) {
            auto &task = currFrame.tasks[taskIndex];
            auto &stat = taskStats[currFrame.taskStatsIndex[taskIndex]];

            if (stat.priorityOrder >= tasksToShow)
                continue;

            if (stat.onScreenIndex == size_t(-1)) {
                stat.onScreenIndex = tasksShownCount++;
            } else
                continue;
            float taskStartHeight = (float(task.startTime) / (1.0f / maxFrameTime)) * legendSize.y;
            float taskEndHeight = (float(task.endTime) / (1.0f / maxFrameTime)) * legendSize.y;

            glm::vec2 markerLeftRectMin = legendPos + glm::vec2(markerLeftRectMargin, legendSize.y);
            glm::vec2 markerLeftRectMax = markerLeftRectMin + glm::vec2(markerLeftRectWidth, 0.0f);
            markerLeftRectMin.y -= taskStartHeight;
            markerLeftRectMax.y -= taskEndHeight;

            glm::vec2 markerRightRectMin = legendPos
                + glm::vec2(markerLeftRectMargin + markerLeftRectWidth + markerMidWidth,
                            legendSize.y - markerRigthRectMargin - (markerRightRectHeight + markerRightRectSpacing) * (float)stat.onScreenIndex);
            glm::vec2 markerRightRectMax = markerRightRectMin + glm::vec2(markerRightRectWidth, -markerRightRectHeight);
            RenderTaskMarker(drawList, markerLeftRectMin, markerLeftRectMax, markerRightRectMin, markerRightRectMax, task.color);

            uint32_t textColor = useColoredLegendText ? task.color : legit::Colors::imguiText; // task.color;

            float taskTimeMs = float(task.endTime - task.startTime);
            std::ostringstream timeText;
            timeText.precision(4);
            timeText << std::fixed << std::string("[") << (taskTimeMs * 1000.0f);

            Text(drawList, markerRightRectMax + textMargin, textColor, timeText.str().c_str());
            Text(drawList, markerRightRectMax + textMargin + glm::vec2(nameOffset, 0.0f), textColor, (std::string("ms] ") + task.name).c_str());
        }

        ImGui::PopClipRect();
    }

    static void Rect(ImDrawList *drawList, glm::vec2 minPoint, glm::vec2 maxPoint, uint32_t col, bool filled = true) {
        if (filled)
            drawList->AddRectFilled(ImVec2(minPoint.x, minPoint.y), ImVec2(maxPoint.x, maxPoint.y), col);
        else
            drawList->AddRect(ImVec2(minPoint.x, minPoint.y), ImVec2(maxPoint.x, maxPoint.y), col);
    }
    static void Text(ImDrawList *drawList, glm::vec2 point, uint32_t col, const char *text) {
        drawList->AddText(ImVec2(point.x, point.y), col, text);
    }
    static void Triangle(ImDrawList *drawList, std::array<glm::vec2, 3> points, uint32_t col, bool filled = true) {
        if (filled)
            drawList->AddTriangleFilled(ImVec2(points[0].x, points[0].y), ImVec2(points[1].x, points[1].y), ImVec2(points[2].x, points[2].y), col);
        else
            drawList->AddTriangle(ImVec2(points[0].x, points[0].y), ImVec2(points[1].x, points[1].y), ImVec2(points[2].x, points[2].y), col);
    }
    static void RenderTaskMarker(
        ImDrawList *drawList,
        glm::vec2 leftMinPoint,
        glm::vec2 leftMaxPoint,
        glm::vec2 rightMinPoint,
        glm::vec2 rightMaxPoint,
        uint32_t col
    ) {
        Rect(drawList, leftMinPoint, leftMaxPoint, col, true);
        Rect(drawList, rightMinPoint, rightMaxPoint, col, true);
        std::array<ImVec2, 4> points = { ImVec2(leftMaxPoint.x, leftMinPoint.y),
                                         ImVec2(leftMaxPoint.x, leftMaxPoint.y),
                                         ImVec2(rightMinPoint.x, rightMaxPoint.y),
                                         ImVec2(rightMinPoint.x, rightMinPoint.y) };
        drawList->AddConvexPolyFilled(points.data(), int(points.size()), col);
    }

    struct FrameData {
        std::vector<legit::ProfilerTask> tasks;
        std::vector<size_t> taskStatsIndex;
    };

    struct TaskStats {
        double maxTime;
        size_t priorityOrder;
        size_t onScreenIndex;
    };
    std::vector<TaskStats> taskStats;
    std::map<std::string, size_t> taskNameToStatsIndex;

    std::vector<FrameData> frames;
    size_t currFrameIndex = 0;
};

class ProfilersWindow {
public:
    ProfilersWindow(): cpuGraph(300), gpuGraph(300) {
        stopProfiling = false;
        frameOffset = 0;
        frameWidth = 3;
        frameSpacing = 1;
        useColoredLegendText = true;
        prevFpsFrameTime = std::chrono::system_clock::now();
        fpsFramesCount = 0;
        avgFrameTime = 1.0f;
    }
    void Render() {
        fpsFramesCount++;
        auto currFrameTime = std::chrono::system_clock::now();
        {
            float fpsDeltaTime = std::chrono::duration<float>(currFrameTime - prevFpsFrameTime).count();
            if (fpsDeltaTime > 0.5f) {
                this->avgFrameTime = fpsDeltaTime / float(fpsFramesCount);
                fpsFramesCount = 0;
                prevFpsFrameTime = currFrameTime;
            }
        }

        std::stringstream title;
        title.precision(2);
        title << std::fixed << "Legit profiler [" << 1.0f / avgFrameTime << "fps\t" << avgFrameTime * 1000.0f << "ms]###ProfilerWindow";
        // ###AnimatedTitle
        ImGui::Begin(title.str().c_str(), nullptr, ImGuiWindowFlags_NoScrollbar);
        ImVec2 canvasSize = ImGui::GetContentRegionAvail();

        int sizeMargin = int(ImGui::GetStyle().ItemSpacing.y);
        int maxGraphHeight = 300;
        int availableGraphHeight = (int(canvasSize.y) - sizeMargin) / 2;
        int graphHeight = std::min(maxGraphHeight, availableGraphHeight);
        int legendWidth = 200;
        int graphWidth = int(canvasSize.x) - legendWidth;
        gpuGraph.RenderTimings(graphWidth, legendWidth, graphHeight, frameOffset);
        cpuGraph.RenderTimings(graphWidth, legendWidth, graphHeight, frameOffset);
        if ((float)graphHeight * 2 + (float)sizeMargin + (float)sizeMargin < canvasSize.y) {
            ImGui::Columns(2);
            ImGui::Checkbox("Stop profiling", &stopProfiling);
            // ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - textSize);
            ImGui::Checkbox("Colored legend text", &useColoredLegendText);
            ImGui::DragInt("Frame offset", &frameOffset, 1.0f, 0, 400);
            ImGui::NextColumn();

            ImGui::SliderInt("Frame width", &frameWidth, 1, 4);
            ImGui::SliderInt("Frame spacing", &frameSpacing, 0, 2);
            ImGui::SliderFloat("Transparency", &ImGui::GetStyle().Colors[ImGuiCol_WindowBg].w, 0.0f, 1.0f);
            ImGui::Columns(1);
        }
        if (!stopProfiling)
            frameOffset = 0;
        gpuGraph.frameWidth = frameWidth;
        gpuGraph.frameSpacing = frameSpacing;
        gpuGraph.useColoredLegendText = useColoredLegendText;
        cpuGraph.frameWidth = frameWidth;
        cpuGraph.frameSpacing = frameSpacing;
        cpuGraph.useColoredLegendText = useColoredLegendText;

        ImGui::End();
    }
    bool stopProfiling;
    int frameOffset;
    ProfilerGraph cpuGraph;
    ProfilerGraph gpuGraph;
    int frameWidth;
    int frameSpacing;
    bool useColoredLegendText;
    using TimePoint = std::chrono::time_point<std::chrono::system_clock>;
    TimePoint prevFpsFrameTime;
    size_t fpsFramesCount;
    float avgFrameTime;
};

} // namespace legit
