// ==================== File Dialogs ====================

std::string Editor::OpenFileDialog(const char* filter) {
#if defined(_WIN32)
    OPENFILENAMEA ofn = {};
    char fileName[MAX_PATH] = "";
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR;
    ofn.lpstrTitle = "Datei öffnen";

    if (GetOpenFileNameA(&ofn)) {
        return std::string(fileName);
    }
    return "";
#else
    // Linux: Use zenity or kdialog
    std::string cmd = "zenity --file-selection --title=\"Datei öffnen\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string result(buffer);
            if (!result.empty() && result.back() == '\n') result.pop_back();
            pclose(pipe);
            return result;
        }
        pclose(pipe);
    }
    return "";
#endif
}

std::string Editor::SaveFileDialog(const char* filter) {
#if defined(_WIN32)
    OPENFILENAMEA ofn = {};
    char fileName[MAX_PATH] = "";
    ofn.lStructSize = sizeof(OPENFILENAMEA);
    ofn.hwndOwner = nullptr;
    ofn.lpstrFilter = filter;
    ofn.lpstrFile = fileName;
    ofn.nMaxFile = MAX_PATH;
    ofn.Flags = OFN_PATHMUSTEXIST | OFN_NOCHANGEDIR | OFN_OVERWRITEPROMPT;
    ofn.lpstrTitle = "Datei speichern";

    if (GetSaveFileNameA(&ofn)) {
        return std::string(fileName);
    }
    return "";
#else
    std::string cmd = "zenity --file-selection --save --title=\"Datei speichern\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string result(buffer);
            if (!result.empty() && result.back() == '\n') result.pop_back();
            pclose(pipe);
            return result;
        }
        pclose(pipe);
    }
    return "";
#endif
}

std::string Editor::SelectFolderDialog() {
#if defined(_WIN32)
    BROWSEINFOA bi = {};
    bi.lpszTitle = "Ordner auswählen";
    bi.ulFlags = BIF_RETURNONLYFSDIRS | BIF_NEWDIALOGSTYLE;
    
    LPITEMIDLIST pidl = SHBrowseForFolderA(&bi);
    if (pidl) {
        char path[MAX_PATH];
        if (SHGetPathFromIDListA(pidl, path)) {
            CoTaskMemFree(pidl);
            return std::string(path);
        }
        CoTaskMemFree(pidl);
    }
    return "";
#else
    std::string cmd = "zenity --file-selection --directory --title=\"Ordner auswählen\" 2>/dev/null";
    FILE* pipe = popen(cmd.c_str(), "r");
    if (pipe) {
        char buffer[1024];
        if (fgets(buffer, sizeof(buffer), pipe)) {
            std::string result(buffer);
            if (!result.empty() && result.back() == '\n') result.pop_back();
            pclose(pipe);
            return result;
        }
        pclose(pipe);
    }
    return "";
#endif
}

// ==================== Gizmo System ====================

void Editor::DrawToolbar() {
    ImGui::Begin("Toolbar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);
    
    // Gizmo mode buttons
    ImGui::Text("Gizmo:");
    ImGui::SameLine();
    
    if (ImGui::RadioButton(Icons::MOUSE_POINTER " Select", mGizmoMode == GizmoMode::None)) {
        mGizmoMode = GizmoMode::None;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(Icons::ARROWS_ALT " Move", mGizmoMode == GizmoMode::Translate)) {
        mGizmoMode = GizmoMode::Translate;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(Icons::SYNC_ALT " Rotate", mGizmoMode == GizmoMode::Rotate)) {
        mGizmoMode = GizmoMode::Rotate;
    }
    ImGui::SameLine();
    if (ImGui::RadioButton(Icons::EXPAND_ARROWS_ALT " Scale", mGizmoMode == GizmoMode::Scale)) {
        mGizmoMode = GizmoMode::Scale;
    }
    
    ImGui::Separator();
    ImGui::SameLine();
    
    // Gizmo space toggle
    if (ImGui::Button(mGizmoSpace == GizmoSpace::Local ? "Local" : "World")) {
        mGizmoSpace = (mGizmoSpace == GizmoSpace::Local) ? GizmoSpace::World : GizmoSpace::Local;
    }
    
    ImGui::Separator();
    ImGui::SameLine();
    
    // Snap toggle
    static bool snapEnabled = true;
    if (ImGui::Checkbox("Snap", &snapEnabled)) {
        // Handle snap toggle
    }
    ImGui::SameLine();
    ImGui::DragFloat("##snap", &mTileScale, 0.1f, 0.1f, 10.0f);
    
    ImGui::End();
}

void Editor::DrawGizmo() {
    if (mSelectedEntity < 0 || mGizmoMode == GizmoMode::None) return;
    if (mEngine.IsPlaying()) return;
    
    auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity));
    if (!transform) return;
    
    Camera& cam = mEngine.GetRenderer().GetCamera();
    Mat4 view = cam.GetViewMatrix();
    Mat4 proj = cam.GetProjectionMatrix();
    
    Vec3 position = transform->transform.position;
    Vec3 rotation = transform->transform.rotation;
    Vec3 scale = transform->transform.scale;
    
    // Draw gizmo axes
    ImDrawList* drawList = ImGui::GetWindowDrawList();
    
    // World to screen projection
    auto worldToScreen = [&](const Vec3& world) -> ImVec2 {
        Vec4 clip = proj * view * Vec4(world, 1.0f);
        if (clip.w == 0) return ImVec2(-1000, -1000);
        
        Vec3 ndc = Vec3(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
        
        ImVec2 viewPos = ImGui::GetCursorScreenPos();
        ImVec2 viewSize = ImGui::GetContentRegionAvail();
        
        float x = (ndc.x * 0.5f + 0.5f) * viewSize.x + viewPos.x;
        float y = (1.0f - (ndc.y * 0.5f + 0.5f)) * viewSize.y + viewPos.y;
        
        return ImVec2(x, y);
    };
    
    // Draw axis lines
    const float axisLen = 2.0f;
    Vec3 origin = position;
    
    // X axis (red)
    Vec3 xEnd = origin + Vec3(axisLen, 0, 0);
    ImVec2 o = worldToScreen(origin);
    ImVec2 xe = worldToScreen(xEnd);
    if (o.x > 0 && xe.x > 0) {
        ImU32 color = (mGizmoAxis == 0) ? IM_COL32(255, 255, 0, 255) : IM_COL32(255, 80, 80, 255);
        drawList->AddLine(o, xe, color, 3.0f);
    }
    
    // Y axis (green)
    Vec3 yEnd = origin + Vec3(0, axisLen, 0);
    ImVec2 ye = worldToScreen(yEnd);
    if (o.x > 0 && ye.x > 0) {
        ImU32 color = (mGizmoAxis == 1) ? IM_COL32(255, 255, 0, 255) : IM_COL32(80, 255, 80, 255);
        drawList->AddLine(o, ye, color, 3.0f);
    }
    
    // Z axis (blue)
    Vec3 zEnd = origin + Vec3(0, 0, axisLen);
    ImVec2 ze = worldToScreen(zEnd);
    if (o.x > 0 && ze.x > 0) {
        ImU32 color = (mGizmoAxis == 2) ? IM_COL32(255, 255, 0, 255) : IM_COL32(80, 120, 255, 255);
        drawList->AddLine(o, ze, color, 3.0f);
    }
    
    // Draw axis labels
    if (o.x > 0) {
        drawList->AddText(ImVec2(xe.x + 5, xe.y), IM_COL32(255, 100, 100, 255), "X");
        drawList->AddText(ImVec2(ye.x + 5, ye.y), IM_COL32(100, 255, 100, 255), "Y");
        drawList->AddText(ImVec2(ze.x + 5, ze.y), IM_COL32(100, 150, 255, 255), "Z");
    }
    
    // Handle gizmo interaction
    ImVec2 mousePos = ImGui::GetMousePos();
    if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && mSceneViewHovered && mSceneViewFocused) {
        // Check which axis was clicked
        // Simplified: just check distance to axis lines
        float bestDist = 100.0f;
        int bestAxis = -1;
        
        for (int i = 0; i < 3; i++) {
            Vec3 axisEnd = origin;
            axisEnd[i] += axisLen;
            ImVec2 axisScreen = worldToScreen(axisEnd);
            float dist = std::sqrt(std::pow(mousePos.x - axisScreen.x, 2) + std::pow(mousePos.y - axisScreen.y, 2));
            if (dist < bestDist && dist < 15.0f) {
                bestDist = dist;
                bestAxis = i;
            }
        }
        
        if (bestAxis >= 0) {
            mGizmoActive = true;
            mGizmoAxis = bestAxis;
            mGizmoStartPos = position;
            mGizmoStartRot = rotation;
            mGizmoStartScale = scale;
            mGizmoStartMousePos = Vec2(mousePos.x, mousePos.y);
        }
    }
    
    if (mGizmoActive && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        ImVec2 delta = ImVec2(mousePos.x - mGizmoStartMousePos.x, mousePos.y - mGizmoStartMousePos.y);
        
        switch (mGizmoMode) {
            case GizmoMode::Translate: {
                // Convert screen delta to world delta
                Camera& cam = mEngine.GetRenderer().GetCamera();
                Vec3 forward = cam.GetForward();
                Vec3 right = cam.GetRight();
                Vec3 up = cam.GetUp();
                
                float sensitivity = 0.01f;
                Vec3 worldDelta = right * (delta.x * sensitivity) - up * (delta.y * sensitivity);
                
                if (mGizmoSpace == GizmoSpace::Local) {
                    // Apply in local space
                    Mat4 rot = glm::rotate(Mat4(1.0f), glm::radians(transform->transform.rotation.y), Vec3(0,1,0));
                    rot = glm::rotate(rot, glm::radians(transform->transform.rotation.x), Vec3(1,0,0));
                    rot = glm::rotate(rot, glm::radians(transform->transform.rotation.z), Vec3(0,0,1));
                    worldDelta = Vec3(rot * Vec4(worldDelta, 0));
                }
                
                // Constrain to selected axis
                if (mGizmoAxis >= 0 && mGizmoAxis <= 2) {
                    Vec3 constrained = worldDelta;
                    for (int i = 0; i < 3; i++) {
                        if (i != mGizmoAxis) constrained[i] = 0;
                    }
                    worldDelta = constrained;
                }
                
                transform->transform.position = mGizmoStartPos + worldDelta;
                break;
            }
            case GizmoMode::Rotate: {
                float sensitivity = 0.5f;
                float rotDelta = (delta.x + delta.y) * sensitivity;
                
                if (mGizmoAxis == 0) transform->transform.rotation.x = mGizmoStartRot.x + rotDelta;
                else if (mGizmoAxis == 1) transform->transform.rotation.y = mGizmoStartRot.y + rotDelta;
                else if (mGizmoAxis == 2) transform->transform.rotation.z = mGizmoStartRot.z + rotDelta;
                break;
            }
            case GizmoMode::Scale: {
                float sensitivity = 0.01f;
                float scaleDelta = 1.0f + (delta.x + delta.y) * sensitivity;
                
                if (mGizmoAxis >= 0 && mGizmoAxis <= 2) {
                    transform->transform.scale = mGizmoStartScale;
                    transform->transform.scale[mGizmoAxis] = mGizmoStartScale[mGizmoAxis] * scaleDelta;
                } else {
                    transform->transform.scale = mGizmoStartScale * scaleDelta;
                }
                break;
            }
            default:
                break;
        }
    }
    
    if (ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        mGizmoActive = false;
        mGizmoAxis = -1;
    }
}

void Editor::DrawStatusBar() {
    ImGui::Begin("StatusBar", nullptr, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove);
    
    ImGui::Text("FPS: %d", mEngine.GetFPS());
    ImGui::SameLine(200);
    ImGui::Text("Entities: %zu", mEngine.GetScene().GetEntities().size());
    ImGui::SameLine(400);
    
    if (mSelectedEntity >= 0) {
        auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity));
        if (transform) {
            ImGui::Text("Pos: %.1f, %.1f, %.1f", 
                transform->transform.position.x, 
                transform->transform.position.y, 
                transform->transform.position.z);
        }
    } else {
        ImGui::Text("No entity selected");
    }
    
    ImGui::SameLine(800);
    ImGui::Text("Gizmo: %s", 
        mGizmoMode == GizmoMode::None ? "Select" : 
        mGizmoMode == GizmoMode::Translate ? "Move" : 
        mGizmoMode == GizmoMode::Rotate ? "Rotate" : "Scale");
    
    ImGui::SameLine(1000);
    ImGui::Text("%s", mGizmoSpace == GizmoSpace::Local ? "Local" : "World");
    
    ImGui::End();
}

void Editor::ShowCrashDialog() {
    if (!mShowCrashDialog) return;
    
    ImGui::OpenPopup("CrashDialog");
    if (ImGui::BeginPopupModal("CrashDialog", &mShowCrashDialog, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
        ImGui::Text("APPLICATION CRASHED");
        ImGui::PopStyleColor();
        ImGui::Separator();
        
        ImGui::TextWrapped("An unexpected error occurred:");
        ImGui::TextWrapped("%s", mLastCrashInfo.message.c_str());
        
        if (!mLastCrashInfo.stackTrace.empty()) {
            ImGui::Separator();
            ImGui::Text("Stack Trace:");
            ImGui::BeginChild("StackTrace", ImVec2(500, 200), true);
            ImGui::TextWrapped("%s", mLastCrashInfo.stackTrace.c_str());
            ImGui::EndChild();
        }
        
        ImGui::Separator();
        ImGui::Text("What would you like to do?");
        
        if (ImGui::Button("Restart Application", ImVec2(200, 0))) {
            mShowCrashDialog = false;
            // Request restart - in a real app this would trigger a proper restart
            mEngine.RequestQuit();
            // Note: Actual restart would need external launcher
        }
        ImGui::SameLine();
        if (ImGui::Button("Quit", ImVec2(200, 0))) {
            mShowCrashDialog = false;
            mEngine.RequestQuit();
        }
        ImGui::SameLine();
        if (ImGui::Button("Continue (Unsafe)", ImVec2(200, 0))) {
            mShowCrashDialog = false;
            // Try to continue - risky but user choice
        }
        
        ImGui::EndPopup();
    }
}

void Editor::DrawUI() {
    // Apply editor theme
    EditorStyle::ApplyTheme(mCurrentTheme);
    
    // Handle global shortcuts
    HandleShortcuts();
    
    // Show crash dialog if needed
    ShowCrashDialog();
    
    // Draw toolbar
    DrawToolbar();
    
    DrawMenuBar();

    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags flags = ImGuiWindowFlags_MenuBar
        | ImGuiWindowFlags_NoDocking
        | ImGuiWindowFlags_NoTitleBar
        | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove
        | ImGuiWindowFlags_NoBringToFrontOnFocus
        | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::Begin("DockSpace", nullptr, flags);

    ImGuiID dockspaceId = ImGui::GetID("MainDockSpace");
    ImGui::DockSpace(dockspaceId, ImVec2(0.0f, 0.0f), ImGuiDockNodeFlags_None);

    if (!mLayoutInitialized) {
        InitializeDefaultLayout(dockspaceId, viewport->Size.x, viewport->Size.y);
        mLayoutInitialized = true;
    }

    ImGui::End();
    ImGui::PopStyleVar(3);

    DrawSceneView();
    DrawHierarchy();
    DrawInspector();
    DrawProjectPanel();
    DrawMapEditor();
    DrawEventEditor();
    DrawScriptEditor();
    if (mAudioPreview) mAudioPreview->DrawUI();
    DrawPrefabBrowser();
    DrawLightingEditor();
    DrawEnvironmentEditor();
    DrawConsole();

    // Draw gizmo in scene view
    DrawGizmo();
    
    // Draw status bar
    DrawStatusBar();

    if (mShowDemo) {
        ImGui::ShowDemoWindow(&mShowDemo);
    }
}

void Editor::HandleShortcuts() {
    ImGuiIO& io = ImGui::GetIO();
    
    // Undo/Redo
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z, false)) {
        if (mEngine.GetCommandHistory().CanUndo()) {
            mEngine.GetCommandHistory().Undo(mEngine);
        }
    }
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y, false)) {
        if (mEngine.GetCommandHistory().CanRedo()) {
            mEngine.GetCommandHistory().Redo(mEngine);
        }
    }
    
    // Delete
    if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) && !io.WantTextInput) {
        DeleteSelectedEntity();
    }
    
    // Play/Stop
    if (ImGui::IsKeyPressed(ImGuiKey_F5, false)) {
        bool isPlaying = mEngine.IsPlaying();
        mEngine.SetPlaying(!isPlaying);
    }
    
    // Focus entity
    if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_F, false)) {
        if (mSelectedEntity >= 0) {
            auto* transform = mEngine.GetScene().GetComponent<TransformComponent>(static_cast<EntityID>(mSelectedEntity));
            if (transform) {
                Camera& cam = mEngine.GetRenderer().GetCamera();
                Vec3 target = transform->transform.position;
                cam.SetPosition(target + Vec3(0, 3, 5));
                cam.SetRotation(Vec3(-30, 0, 0));
            }
        }
    }
    
    // Gizmo mode shortcuts
    if (ImGui::IsKeyPressed(ImGuiKey_Q, false)) mGizmoMode = GizmoMode::None;
    if (ImGui::IsKeyPressed(ImGuiKey_W, false) && !io.WantTextInput) mGizmoMode = GizmoMode::Translate;
    if (ImGui::IsKeyPressed(ImGuiKey_E, false) && !io.WantTextInput) mGizmoMode = GizmoMode::Rotate;
    if (ImGui::IsKeyPressed(ImGuiKey_R, false) && !io.WantTextInput) mGizmoMode = GizmoMode::Scale;
    
    // Toggle space
    if (ImGui::IsKeyPressed(ImGuiKey_X, false)) {
        mGizmoSpace = (mGizmoSpace == GizmoSpace::Local) ? GizmoSpace::World : GizmoSpace::Local;
    }
}

void Editor::UpdateGizmo() {
    // Called from DrawGizmo
}

void Editor::DrawGizmoAxis(const Vec3& position, const Mat4& view, const Mat4& proj, const Vec2& viewPos, const Vec2& viewSize) {
    // Implementation in DrawGizmo
}

bool Editor::GizmoIntersect(const Vec2& mousePos, const Vec2& viewPos, const Vec2& viewSize, Vec3& outAxis) {
    // Raycast against gizmo axes
    return false;
}

} // namespace rpg