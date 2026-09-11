#pragma once

void draw_menu(variables_s& info)
{
	ImGui::Begin("test_menu");
	ImGui::Text("rendered from dll present hook");

	ImGui::Separator();

	ImGui::Text("SwapChain:        %p", info.pSwapChain);

	ImGui::Separator();

	ImGui::Text("Device:           %p", info.pDevice);

	ImGui::Separator();

	ImGui::Text("Context:          %p", info.pContext);

	ImGui::Separator();

	ImGui::Text("Base Address:     %p", (void*)info.base);

	ImGui::Separator();

	ImGui::Text("Original Present: %p", (void*)info.original_present);

	ImGui::Separator();

	ImGui::Text("Current Present:  %p", (void*)info.current_present);

	ImGui::Separator();



	ImGui::End();
}

