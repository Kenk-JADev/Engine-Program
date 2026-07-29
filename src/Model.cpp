#include "rpgmaker3d/Model.h"
#include "rpgmaker3d/Logger.h"
#include <glad/gl.h>
#include <fstream>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <cmath>
#include <cctype>
#include <vector>

namespace rpg {

void Mesh::BuildGPU() {
    if (mVAO) Delete();

    glGenVertexArrays(1, &mVAO);
    glGenBuffers(1, &mVBO);
    glGenBuffers(1, &mEBO);

    glBindVertexArray(mVAO);

    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex), vertices.data(),
                 dynamicDraw ? GL_DYNAMIC_DRAW : GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, mEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned int), indices.data(), GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));

    glBindVertexArray(0);
}

void Mesh::UpdateVertices() {
    // PAKET 46: CPU-Morph-Stand hochladen. Kein GL (headless) oder statisch
    // angelegt -> nichts zu tun.
    if (mVBO == 0 || !dynamicDraw) return;
    glBindBuffer(GL_ARRAY_BUFFER, mVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    vertices.size() * sizeof(Vertex), vertices.data());
    glBindBuffer(GL_ARRAY_BUFFER, 0);
}

void Mesh::Draw() const {
    glBindVertexArray(mVAO);
    glDrawElements(GL_TRIANGLES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::DrawLines() const {
    glBindVertexArray(mVAO);
    glDrawElements(GL_LINES, static_cast<GLsizei>(indices.size()), GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void Mesh::Delete() {
    if (mVAO) glDeleteVertexArrays(1, &mVAO);
    if (mVBO) glDeleteBuffers(1, &mVBO);
    if (mEBO) glDeleteBuffers(1, &mEBO);
    mVAO = mVBO = mEBO = 0;
}

Model::Model() = default;

Model::~Model() {
    Delete();
}

namespace {

// PAKET 46: OBJ -> Mesh (CPU-Seite, ohne GPU-Upload). Aus LoadFromOBJ
// herausgeloest, damit das Anim-Manifest dieselbe Parse-Logik fuer alle
// Frames nutzt. Ungueltige Facetten-Indizes liefern false (vorher:
// stille UB-Gefahr durch positions[p-1] ohne Bereichspruefung).
bool ParseOBJToMesh(const std::string& path, Mesh& mesh, std::string& err) {
    std::ifstream file(path);
    if (!file.is_open()) {
        err = "kann nicht geoeffnet werden";
        return false;
    }

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> uvs;

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream iss(line);
        std::string prefix;
        iss >> prefix;

        if (prefix == "v") {
            Vec3 v;
            iss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        } else if (prefix == "vn") {
            Vec3 n;
            iss >> n.x >> n.y >> n.z;
            normals.push_back(n);
        } else if (prefix == "vt") {
            Vec2 t;
            iss >> t.x >> t.y;
            uvs.push_back(t);
        } else if (prefix == "f") {
            std::string face;
            std::vector<unsigned int> faceIndices;
            bool badFace = false;
            while (iss >> face) {
                std::replace(face.begin(), face.end(), '/', ' ');
                std::istringstream fss(face);
                int p = 0, t = 0, n = 0;
                fss >> p >> t >> n;
                if (p < 1 || (size_t)p > positions.size()) {
                    badFace = true; // Verweis auf nicht vorhandene Position
                    break;
                }

                Vertex v;
                v.position = positions[(size_t)p - 1];
                v.normal = (n > 0 && (size_t)n <= normals.size())
                               ? normals[(size_t)n - 1] : Vec3(0, 1, 0);
                v.uv = (t > 0 && (size_t)t <= uvs.size())
                           ? uvs[(size_t)t - 1] : Vec2(0, 0);

                mesh.vertices.push_back(v);
                faceIndices.push_back(static_cast<unsigned int>(mesh.vertices.size() - 1));
            }
            if (badFace) {
                err = "Facette verweist auf fehlende Position";
                return false;
            }
            // Triangulate simple convex faces (fan triangulation)
            for (size_t i = 1; i + 1 < faceIndices.size(); ++i) {
                mesh.indices.push_back(faceIndices[0]);
                mesh.indices.push_back(faceIndices[i]);
                mesh.indices.push_back(faceIndices[i + 1]);
            }
        }
    }

    if (mesh.vertices.empty()) {
        err = "keine Geometrie";
        return false;
    }
    return true;
}

} // namespace

bool Model::LoadFromOBJ(const std::string& path) {
    Mesh mesh;
    std::string err;
    if (!ParseOBJToMesh(path, mesh, err)) {
        std::cerr << "Failed to load OBJ: " << path << " (" << err << ")" << std::endl;
        return false;
    }
    mesh.BuildGPU();
    mMeshes.push_back(std::move(mesh));
    return true;
}

void Model::AddMesh(Mesh&& mesh) {
    mMeshes.push_back(std::move(mesh));
}

// ---------------------------------------------------------------------------
// PAKET 46 (Etappe 3, Stufe 1): Keyframe-Morph-Animation
// ---------------------------------------------------------------------------
// Manifest "<name>.anim" im INI-Stil (Parser-Kultur wie Game.ini):
//
//   [frames]                ; zeilenweise, Reihenfolge = Frame-Index
//   f0 = ritter_idle0.obj
//   f1 = ritter_idle1.obj
//   f2 = ritter_walk0.obj
//
//   [clip:idle]
//   frames = 0,1            ; Indexliste (Ping-Pong: 0,1,0 moeglich)
//   fps = 3                 ; Schritte/Sekunde (0.1..120)
//   loop = 1                ; 0 = am letzten Frame stehen bleiben
//   start = 1               ; beim Laden automatisch abspielen (immer explizit)
//
// Pfade sind relativ zum Manifest-Ordner. ALLE Frames brauchen identische
// Topologie (gleiche Vertex-/Indexzahl pro Mesh) - so exportieren, wie es
// Morph-Keyframes in Blender/Co erzeugen ("Export selected frame range").
// ---------------------------------------------------------------------------
namespace {

struct PendingAnimClipDef {
    AnimClip clip;
    std::string framesRaw;
    bool start = false;
};

std::string AnimDirOf(const std::string& path) {
    const size_t slash = path.find_last_of("/\\");
    return (slash == std::string::npos) ? std::string(".") : path.substr(0, slash);
}

} // namespace

bool Model::LoadAnimManifest(const std::string& path) {
    std::ifstream f(path);
    if (!f.is_open()) {
        RPG_LOG_WARN("Anim-Manifest nicht gefunden: " + path);
        return false;
    }
    const std::string baseDir = AnimDirOf(path);

    auto lower = [](std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        return s;
    };
    auto trim = [](std::string& s) {
        while (!s.empty() && std::isspace((unsigned char)s.front())) s.erase(s.begin());
        while (!s.empty() && std::isspace((unsigned char)s.back())) s.pop_back();
    };
    auto toBool = [&](std::string v, bool fallback) {
        trim(v); v = lower(v);
        if (v == "1" || v == "true" || v == "yes" || v == "on") return true;
        if (v == "0" || v == "false" || v == "no" || v == "off") return false;
        return fallback;
    };
    auto toFloat = [&](std::string v, float fallback) {
        trim(v);
        try { return std::stof(v); } catch (...) { return fallback; }
    };

    std::vector<std::string> frameFiles;
    std::vector<PendingAnimClipDef> pending;
    int section = 0; // 1=[frames], 2=[clip:*]

    std::string line;
    while (std::getline(f, line)) {
        trim(line);
        if (line.empty() || line[0] == ';' || line[0] == '#') continue;
        if (line[0] == '[') {
            const size_t close = line.find(']');
            const std::string sec = lower(line.substr(1,
                close == std::string::npos ? close : close - 1));
            if (sec == "frames") {
                section = 1;
            } else if (sec.rfind("clip:", 0) == 0) {
                PendingAnimClipDef def;
                def.clip.name = line.substr(6, (close == std::string::npos
                                                    ? std::string::npos : close - 6));
                trim(def.clip.name);
                pending.push_back(def);
                section = 2;
            } else {
                section = 0;
            }
            continue;
        }
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = lower(line.substr(0, eq));
        trim(key);
        std::string value = line.substr(eq + 1);
        const size_t cmt = value.find(';');
        if (cmt != std::string::npos) value = value.substr(0, cmt);
        trim(value);

        if (section == 1) {
            if (!value.empty()) frameFiles.push_back(value); // Reihenfolge = Index
        } else if (section == 2 && !pending.empty()) {
            PendingAnimClipDef& def = pending.back();
            if (key == "frames")    def.framesRaw = value;
            else if (key == "fps")  def.clip.fps = std::clamp(toFloat(value, def.clip.fps), 0.1f, 120.0f);
            else if (key == "loop") def.clip.loop = toBool(value, def.clip.loop);
            else if (key == "start") def.start = toBool(value, def.start);
        }
    }

    if (frameFiles.empty()) {
        RPG_LOG_WARN("Anim-Manifest ohne [frames]: " + path);
        return false;
    }

    // Indexliste der Clips aufloesen + pruefen
    auto parseIndexList = [](const std::string& raw, std::vector<int>& out,
                             int maxIndex) {
        out.clear();
        std::istringstream ss(raw);
        std::string tok;
        while (std::getline(ss, tok, ',')) {
            try {
                size_t used = 0;
                const int v = std::stoi(tok, &used);
                if (used == 0) return false;
                if (v < 0 || v >= maxIndex) return false;
                out.push_back(v);
            } catch (...) {
                return false;
            }
        }
        return !out.empty();
    };
    std::vector<AnimClip> clips;
    int startClip = -1;
    for (const PendingAnimClipDef& def : pending) {
        AnimClip clip = def.clip;
        if (!parseIndexList(def.framesRaw, clip.frames, (int)frameFiles.size())) {
            RPG_LOG_WARN("Anim-Clip '" + clip.name + "' ohne gueltige frames= in " + path);
            continue;
        }
        if (def.start && startClip < 0) startClip = (int)clips.size();
        clips.push_back(std::move(clip));
    }

    // Frames laden + Topologie gegen Frame 0 pruefen (eine Mesh-Ebene pro
    // OBJ - der Engine-Loader ist ohnehin Einzel-Mesh).
    std::vector<std::vector<Vertex>> frameVerts;
    std::vector<unsigned int> baseIndices;
    for (size_t i = 0; i < frameFiles.size(); ++i) {
        const std::string full = baseDir + "/" + frameFiles[i];
        Mesh tmp;
        std::string err;
        if (!ParseOBJToMesh(full, tmp, err)) {
            RPG_LOG_WARN("Anim-Frame [" + std::to_string(i) + "] " + full +
                         " ladefehler: " + err);
            return false;
        }
        if (i == 0) {
            baseIndices = tmp.indices;
        } else if (tmp.vertices.size() != frameVerts[0].size() ||
                   tmp.indices.size() != baseIndices.size()) {
            RPG_LOG_WARN("Anim-Frame [" + std::to_string(i) + "] Topologie weicht ab (" +
                         std::to_string(tmp.vertices.size()) + "/" +
                         std::to_string(tmp.indices.size()) + " vs. " +
                         std::to_string(frameVerts[0].size()) + "/" +
                         std::to_string(baseIndices.size()) + "): " + full);
            return false;
        }
        frameVerts.push_back(std::move(tmp.vertices));
    }

    // Uebernehmen: Basismesh = Frame 0 mit dynamischem Upload (alte GPU-
    // Handles sauber freigeben), Clips + Startzustand setzen.
    Delete();
    Mesh base;
    base.vertices = frameVerts[0];
    base.indices = baseIndices;
    base.dynamicDraw = true;
    base.BuildGPU();
    mMeshes.push_back(std::move(base));

    mFrames.clear();
    for (auto& verts : frameVerts)
        mFrames.push_back({verts}); // eine Mesh-Ebene (Einzel-Mesh-OBJs)
    mClips = std::move(clips);
    mClipIndex = -1;
    mClipPlaying = false;
    mClipTime = 0.0f;

    RPG_LOG_INFO("Anim-Manifest geladen: " + path + " (" +
                 std::to_string(mFrames.size()) + " Frames, " +
                 std::to_string(mClips.size()) + " Clips)");
    if (startClip >= 0) PlayClip(startClip);
    return true;
}

bool Model::LoadAnyModelFile(const std::string& path) {
    std::string ext;
    const size_t dot = path.find_last_of('.');
    if (dot != std::string::npos) ext = path.substr(dot);
    std::transform(ext.begin(), ext.end(), ext.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    if (ext == ".anim") return LoadAnimManifest(path);
    return LoadFromOBJ(path);
}

int Model::FindClip(const std::string& name) const {
    for (size_t i = 0; i < mClips.size(); ++i)
        if (mClips[i].name == name) return (int)i;
    return -1;
}

const AnimClip* Model::GetClip(int index) const {
    if (index < 0 || index >= (int)mClips.size()) return nullptr;
    return &mClips[(size_t)index];
}

void Model::PlayClip(int clipIndex, bool restart) {
    const AnimClip* clip = GetClip(clipIndex);
    if (!clip || clip->frames.empty() || mFrames.empty()) {
        RPG_LOG_WARN("Model::PlayClip - unbekannter Clip " + std::to_string(clipIndex));
        return;
    }
    if (mClipIndex == clipIndex && mClipPlaying && !restart) return;
    mClipIndex = clipIndex;
    mClipPlaying = true;
    mClipTime = 0.0f;
    ApplyMorph(clip->frames[0], clip->frames[0], 0.0f); // Sprung sofort sichtbar
}

void Model::PlayClip(const std::string& name, bool restart) {
    PlayClip(FindClip(name), restart);
}

void Model::StopClip() {
    mClipPlaying = false;
}

void Model::UpdateAnimation(float dt) {
    if (!mClipPlaying) return;
    const AnimClip* clip = GetClip(mClipIndex);
    if (!clip || clip->frames.empty() || clip->fps <= 0.0f) {
        mClipPlaying = false;
        return;
    }
    const int count = (int)clip->frames.size();
    if (count == 1) {
        ApplyMorph(clip->frames[0], clip->frames[0], 0.0f);
        if (!clip->loop) mClipPlaying = false;
        return;
    }
    mClipTime += dt;
    const float stepF = mClipTime * clip->fps;
    const int last = count - 1;
    if (clip->loop) {
        const int fi = (int)std::floor(stepF);
        const float t = stepF - std::floor(stepF);
        ApplyMorph(clip->frames[fi % count], clip->frames[(fi + 1) % count], t);
    } else if (stepF >= (float)last) {
        ApplyMorph(clip->frames[last], clip->frames[last], 0.0f);
        mClipPlaying = false; // nicht-loopend: am Ende stehen bleiben
    } else {
        const int fi = (int)std::floor(stepF);
        ApplyMorph(clip->frames[fi], clip->frames[fi + 1], stepF - (float)fi);
    }
}

void Model::ApplyMorph(int frameA, int frameB, float t) {
    if (mFrames.empty() || mMeshes.empty()) return;
    frameA = std::clamp(frameA, 0, (int)mFrames.size() - 1);
    frameB = std::clamp(frameB, 0, (int)mFrames.size() - 1);
    t = std::clamp(t, 0.0f, 1.0f);
    const auto& fa = mFrames[(size_t)frameA];
    const auto& fb = mFrames[(size_t)frameB];
    const size_t meshCount = std::min({fa.size(), fb.size(), mMeshes.size()});
    for (size_t m = 0; m < meshCount; ++m) {
        auto& verts = mMeshes[m].vertices;
        const size_t n = std::min({verts.size(), fa[m].size(), fb[m].size()});
        for (size_t i = 0; i < n; ++i) {
            const Vertex& a = fa[m][i];
            const Vertex& b = fb[m][i];
            Vertex v;
            v.position = a.position + (b.position - a.position) * t;
            v.normal = a.normal + (b.normal - a.normal) * t;
            const float len = std::sqrt(v.normal.x * v.normal.x +
                                        v.normal.y * v.normal.y +
                                        v.normal.z * v.normal.z);
            if (len > 1e-6f) v.normal = v.normal * (1.0f / len);
            v.uv = a.uv; // UVs aus Frame A (Morph bewegt nur Geometrie)
            verts[i] = v;
        }
        mMeshes[m].UpdateVertices();
    }
}

void Model::Draw() const {
    for (const auto& mesh : mMeshes) {
        mesh.Draw();
    }
}

void Model::Delete() {
    for (auto& mesh : mMeshes) {
        mesh.Delete();
    }
    mMeshes.clear();
}

Mesh MeshFactory::CreateCube(float size) {
    Mesh mesh;
    float h = size * 0.5f;

    mesh.vertices = {
        // Front
        {{-h, -h,  h}, {0, 0, 1}, {0, 0}}, {{ h, -h,  h}, {0, 0, 1}, {1, 0}}, {{ h,  h,  h}, {0, 0, 1}, {1, 1}}, {{-h,  h,  h}, {0, 0, 1}, {0, 1}},
        // Back
        {{ h, -h, -h}, {0, 0, -1}, {0, 0}}, {{-h, -h, -h}, {0, 0, -1}, {1, 0}}, {{-h,  h, -h}, {0, 0, -1}, {1, 1}}, {{ h,  h, -h}, {0, 0, -1}, {0, 1}},
        // Top
        {{-h,  h,  h}, {0, 1, 0}, {0, 0}}, {{ h,  h,  h}, {0, 1, 0}, {1, 0}}, {{ h,  h, -h}, {0, 1, 0}, {1, 1}}, {{-h,  h, -h}, {0, 1, 0}, {0, 1}},
        // Bottom
        {{-h, -h, -h}, {0, -1, 0}, {0, 0}}, {{ h, -h, -h}, {0, -1, 0}, {1, 0}}, {{ h, -h,  h}, {0, -1, 0}, {1, 1}}, {{-h, -h,  h}, {0, -1, 0}, {0, 1}},
        // Right
        {{ h, -h,  h}, {1, 0, 0}, {0, 0}}, {{ h, -h, -h}, {1, 0, 0}, {1, 0}}, {{ h,  h, -h}, {1, 0, 0}, {1, 1}}, {{ h,  h,  h}, {1, 0, 0}, {0, 1}},
        // Left
        {{-h, -h, -h}, {-1, 0, 0}, {0, 0}}, {{-h, -h,  h}, {-1, 0, 0}, {1, 0}}, {{-h,  h,  h}, {-1, 0, 0}, {1, 1}}, {{-h,  h, -h}, {-1, 0, 0}, {0, 1}},
    };

    mesh.indices = {
        0, 1, 2, 2, 3, 0,
        4, 5, 6, 6, 7, 4,
        8, 9, 10, 10, 11, 8,
        12, 13, 14, 14, 15, 12,
        16, 17, 18, 18, 19, 16,
        20, 21, 22, 22, 23, 20
    };

    mesh.BuildGPU();
    return mesh;
}

Mesh MeshFactory::CreatePlane(float size) {
    Mesh mesh;
    float h = size * 0.5f;
    mesh.vertices = {
        {{-h, 0, -h}, {0, 1, 0}, {0, 0}},
        {{ h, 0, -h}, {0, 1, 0}, {1, 0}},
        {{ h, 0,  h}, {0, 1, 0}, {1, 1}},
        {{-h, 0,  h}, {0, 1, 0}, {0, 1}}
    };
    mesh.indices = {0, 1, 2, 2, 3, 0};
    mesh.BuildGPU();
    return mesh;
}

Mesh MeshFactory::CreateQuad(float width, float height) {
    Mesh mesh;
    float hw = width * 0.5f;
    float hh = height * 0.5f;
    mesh.vertices = {
        {{-hw, -hh, 0}, {0, 0, 1}, {0, 0}},
        {{ hw, -hh, 0}, {0, 0, 1}, {1, 0}},
        {{ hw,  hh, 0}, {0, 0, 1}, {1, 1}},
        {{-hw,  hh, 0}, {0, 0, 1}, {0, 1}}
    };
    mesh.indices = {0, 1, 2, 2, 3, 0};
    mesh.BuildGPU();
    return mesh;
}

Mesh MeshFactory::CreateGrid(int lines, float spacing) {
    Mesh mesh;
    float half = lines * spacing * 0.5f;
    for (int i = 0; i <= lines; ++i) {
        float p = -half + i * spacing;
        Vertex v1{{-half, 0, p}, {0, 1, 0}, {0, 0}};
        Vertex v2{{half, 0, p}, {0, 1, 0}, {1, 0}};
        Vertex v3{{p, 0, -half}, {0, 1, 0}, {0, 0}};
        Vertex v4{{p, 0, half}, {0, 1, 0}, {1, 0}};

        mesh.vertices.push_back(v1); mesh.vertices.push_back(v2);
        mesh.vertices.push_back(v3); mesh.vertices.push_back(v4);

        unsigned int base = static_cast<unsigned int>(mesh.vertices.size()) - 4;
        mesh.indices.push_back(base); mesh.indices.push_back(base + 1);
        mesh.indices.push_back(base + 2); mesh.indices.push_back(base + 3);
    }
    mesh.BuildGPU();
    return mesh;
}

} // namespace rpg
