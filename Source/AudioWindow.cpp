#include "AudioWindow.h"
#include "Brofiler\Brofiler.h"

AudioWindow::AudioWindow()
{
	active = false;
	window_name = "Audio";
}

AudioWindow::~AudioWindow()
{
}

void AudioWindow::DrawWindow()
{
	BROFILER_CATEGORY("Editor - Audio: DrawWindow", Profiler::Color::Black);

	ImGui::Begin(window_name.c_str(), &active,
		ImGuiWindowFlags_AlwaysAutoResize |
		ImGuiWindowFlags_NoCollapse |
		ImGuiWindowFlags_ShowBorders);
	ImGui::SetWindowFontScale(1.1f);
	ImGui::Text("Audio Engine");
	ImGui::SetWindowFontScale(1);
	ImGui::Text("The next generation 3D Game Engine");

	ImGui::End();
}
