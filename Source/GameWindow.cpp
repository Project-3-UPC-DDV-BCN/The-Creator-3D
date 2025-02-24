#include "GameWindow.h"
#include "Application.h"
#include "ModuleRenderer3D.h"
#include "RenderTextureMSAA.h"
#include "ComponentCamera.h"
#include "ModuleWindow.h"

GameWindow::GameWindow()
{
	active = true;
	window_name = "Game";
	game_scene_width = 0;
	game_scene_height = 0;
	window_pos = float2::zero;
}

GameWindow::~GameWindow()
{
}

void GameWindow::DrawWindow()
{
	BROFILER_CATEGORY("Editor - Game: DrawWindow", Profiler::Color::Black);

	if (App->IsGame())
	{
		float width = App->window->GetWidth();
		float height = App->window->GetHeight();

		ImGui::SetNextWindowSize({ width + 20, height + 20 });
		ImGui::SetNextWindowPos({ -10,-10 });
		ImGui::Begin("Hello", &active, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);

		if (game_scene_width != width || game_scene_height != height)
		{
			if (App->renderer3D->game_camera != nullptr)
			{
				App->renderer3D->OnResize(width, height, App->renderer3D->game_camera);
			}
			game_scene_width = width;
			game_scene_height = height;
		}

		if (App->renderer3D->game_camera != nullptr && App->renderer3D->game_camera->GetViewportTexture() != nullptr)
		{
			ImGui::Image((void*)App->renderer3D->game_camera->GetViewportTexture()->GetTextureID(), { width, height }, ImVec2(0, 1), ImVec2(1, 0));
		}

		ImGui::End();
	}
	else
	{
		BROFILER_CATEGORY("game window draw", Profiler::Color::BlanchedAlmond);
		if (ImGui::BeginDock(window_name.c_str(), false, false, false,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse))
		{
			ImVec2 size = ImGui::GetContentRegionAvail();
			window_pos = float2(ImGui::GetWindowPos().x, ImGui::GetWindowPos().y);

			if (game_scene_width != size.x || game_scene_height != size.y)
			{
				if (App->renderer3D->game_camera != nullptr)
				{
					App->renderer3D->OnResize(size.x, size.y, App->renderer3D->game_camera);
				}
				game_scene_width = size.x;
				game_scene_height = size.y;
			}

			if (App->renderer3D->game_camera != nullptr && App->renderer3D->game_camera->GetViewportTexture() != nullptr)
			{
				ImGui::Image((void*)App->renderer3D->game_camera->GetViewportTexture()->GetTextureID(), size, ImVec2(0, 1), ImVec2(1, 0));
			}
		}
		ImGui::EndDock();
	}
}

float2 GameWindow::GetPos() const
{
	return window_pos;
}

float2 GameWindow::GetSize() const
{
	if (!App->IsGame())
		return float2(game_scene_width, game_scene_height);
	else
		return float2(App->window->GetWidth(), App->window->GetHeight());
}
