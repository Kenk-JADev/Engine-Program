#pragma once

#include <string>
#include <vector>
#include "Types.h"

typedef unsigned int GLuint;

namespace rpg {

class Mesh {
public:
    Mesh() = default;
    ~Mesh() { Delete(); }

    // Kein Kopieren, nur Verschieben (OpenGL-Handles sind einzigartig)
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    Mesh(Mesh&& other) noexcept
        : vertices(std::move(other.vertices)),
          indices(std::move(other.indices)),
          textureName(std::move(other.textureName)),
          dynamicDraw(other.dynamicDraw),
          mVAO(other.mVAO), mVBO(other.mVBO), mEBO(other.mEBO) {
        other.mVAO = other.mVBO = other.mEBO = 0;
    }

    Mesh& operator=(Mesh&& other) noexcept {
        if (this != &other) {
            Delete();
            vertices = std::move(other.vertices);
            indices = std::move(other.indices);
            textureName = std::move(other.textureName);
            dynamicDraw = other.dynamicDraw;
            mVAO = other.mVAO; mVBO = other.mVBO; mEBO = other.mEBO;
            other.mVAO = other.mVBO = other.mEBO = 0;
        }
        return *this;
    }

    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    std::string textureName;
    /// PAKET 46 (Keyframe-Morph): true -> VBO wird als GL_DYNAMIC_DRAW
    /// angelegt; nach CPU-Morph per UpdateVertices() neu hochladen.
    bool dynamicDraw = false;

    void BuildGPU();
    /// PAKET 46: aktuellen CPU-Vertexstand in den dynamischen VBO laden.
    /// No-Op ohne VBO (headless-Lauf) oder wenn dynamicDraw == false.
    void UpdateVertices();
    void Draw() const;
    /// Draw as GL_LINES (for grids / debug lines). Indices must form line pairs.
    void DrawLines() const;
    void Delete();

private:
    GLuint mVAO = 0;
    GLuint mVBO = 0;
    GLuint mEBO = 0;
};

// PAKET 46 (Etappe 3, Stufe 1): benannter Keyframe-Clip eines Morph-Modells.
// frames = Indizes in die Frame-Liste des Manifests (.anim), fps = Schritte
// pro Sekunde; loop=0: Clip bleibt am letzten Frame stehen.
struct AnimClip {
    std::string name;
    std::vector<int> frames;
    float fps = 8.0f;
    bool loop = true;
};

class Model {
public:
    Model();
    ~Model();

    bool LoadFromOBJ(const std::string& path);
    /// PAKET 46: Keyframe-Morph-Manifest (.anim, INI-Stil wie Game.ini).
    /// Laedt die Frame-OBJs (identische Topologie Pflicht) und legt das
    /// Basismesh als dynamischen Upload an. Pose/Clips dann per
    /// PlayClip/UpdateAnimation. Liefert false bei Parse-/Topologiefehlern
    /// (Log), die bisherigen Meshes bleiben dann unberuehrt.
    bool LoadAnimManifest(const std::string& path);
    /// Endung-Vermittlung: ".anim" -> LoadAnimManifest, sonst LoadFromOBJ.
    /// (ResourceManager/Editor rufen diese Variante.)
    bool LoadAnyModelFile(const std::string& path);
    void AddMesh(Mesh&& mesh);
    void Draw() const;
    void Delete();

    size_t GetMeshCount() const { return mMeshes.size(); }
    Mesh& GetMesh(size_t index) { return mMeshes[index]; }
    const Mesh& GetMesh(size_t index) const { return mMeshes[index]; }

    // ---- PAKET 46: Clip-Laufzeit ----
    /// true, wenn ein Manifest mit Frames geladen wurde.
    bool IsAnimated() const { return !mFrames.empty(); }
    int FindClip(const std::string& name) const; // -1 = unbekannt
    const AnimClip* GetClip(int index) const;    // nullptr ausserhalb
    int GetClipCount() const { return (int)mClips.size(); }
    /// Clip starten (restart=false: nur wechseln, wenn nicht schon aktiv).
    void PlayClip(int clipIndex, bool restart = true);
    void PlayClip(const std::string& name, bool restart = true);
    void StopClip();                    // aktuelle Pose bleibt stehen
    int ActiveClip() const { return mClipIndex; }
    bool IsClipPlaying() const { return mClipPlaying; }
    /// Zeit weitertreiben + Morph anwenden (Template-Ebene).
    /// NUR noch API-Komplettheit (PAKET 46); die Szene tickt seit PAKET 47
    /// je Entitaet (Instanz-Pose) — die Template-Pose dient nicht mehr als
    /// Renderquelle, wenn Instanzpuffer existieren.
    void UpdateAnimation(float dt);

    // ---- PAKET 47 (Etappe 3, Stufe 2): Instanz-Verwendung ----
    /// Manifest-Clip mit start=1 (-1 = keiner). Die Szene stoesst diesen
    /// Clip je Entitaet selbst an (das Template BLEIBT statisch auf Frame 0).
    int GetAutostartClip() const { return mAutostartClip; }
    /// Reine Clip-Zeitrechnung (von jeder Pose losgeloest): time/playing
    /// werden veraendert; outA/outB/outT liefern die abzuspielende Pose.
    /// Beim letzten nicht-loopenden Frame endet playing und outA==outB==Endframe.
    static bool AdvanceClipState(const AnimClip& clip, float dt,
                                 float& time, bool& playing,
                                 int& outA, int& outB, float& outT);
    /// Schreibt die Morph-Pose in ein FREMDES Mesh (Instanzpuffer).
    /// dst braucht passende Topologie (Scene legt sie aus der Frame-Basis an)
    /// und uploaded bei Bedarf (dynamicDraw). false bei Frame-/Indexfehlern.
    bool MorphToMesh(size_t meshIndex, int frameA, int frameB, float t,
                     Mesh& dst) const;

private:
    /// frameA/frameB mit t (0..1) mischen und hochladen (CPU-Morph,
    /// RPG-taugliche Vertexzahlen). Guarded gegen Frame-/Groessenfehler.
    void ApplyMorph(int frameA, int frameB, float t);

    std::vector<Mesh> mMeshes;
    // mFrames[f][m] = Vertexliste von Clip-Frame f, Mesh m. Die Indizes
    // teilen sich alle Frames mit dem Basismesh (Topologie validiert).
    std::vector<std::vector<std::vector<Vertex>>> mFrames;
    std::vector<AnimClip> mClips;
    int mClipIndex = -1;
    bool mClipPlaying = false;
    float mClipTime = 0.0f;
    int mAutostartClip = -1; // PAKET 47: start=1-Mark, Szene nutzt je Entitaet
};

class MeshFactory {
public:
    static Mesh CreateCube(float size = 1.0f);
    static Mesh CreatePlane(float size = 1.0f);
    static Mesh CreateQuad(float width = 1.0f, float height = 1.0f);
    static Mesh CreateGrid(int lines, float spacing);
};

} // namespace rpg
