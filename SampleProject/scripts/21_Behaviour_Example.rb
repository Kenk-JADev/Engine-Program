# @name    Behaviour-Beispiel (RubyBehaviour, SADS Kap. 8)
# @version 1.0
# @author  RPG Maker 3D
# @desc    Referenz fuer Entity-Behaviours: Klassen-Logik pro 3D-Entity.
#
# So funktioniert es:
#   1. Eine Entity im Qt-Editor (oder per scene.json) bekommt die
#      ScriptComponent mit className "NpcBobber" — fertig.
#   2. Zur Laufzeit erzeugt die Engine automatisch EINE Instanz der Klasse
#      pro Entity, ruft EINMAL start() und dann pro Frame update(dt).
#   3. Alternativ zur Laufzeit anhaengen:
#        Entity.attach_behaviour(Entity.find_by_name("Kiste"), "NpcBobber")
#      und wieder loesen:
#        Entity.detach_behaviour(id)
#
# Lifecycle (SADS Kap. 9): update(dt) laeuft variabel pro Frame;
# fixed_update() liegt auf dem XP-Logiktakt (40 Hz), late_update() nach dem
# normalen Szenen-Update. Alle drei sind optional.

class NpcBobber < Behaviour
  def start
    @t = 0.0
    @base_y = entity_position ? entity_position[1] : 0.0
  end

  def update(dt)
    @t += dt
    pos = entity_position
    return unless pos
    # Sanftes Schweben: y = Basis + Sinus
    set_entity_position(pos[0], @base_y + 0.15 * Math.sin(@t * 2.0), pos[2])
    rotate(0.0, dt * 45.0, 0.0) # langsame Drehung (Grad/Sekunde)
  end

  def fixed_update
    # 40-Hz-Logik (deterministisch, framerate-unabhaengig) — z. B. simple AI
  end
end

# Zweites Beispiel: idle-Clip-Autostart (Morph-Animation, PAKET 46/47)
class ClipGreeter < Behaviour
  def start
    start_clip("idle", true)
  end
end

# Drittes Beispiel: Sichtlinie per Physics.raycast (SADS Kap. 11)
class Lookout < Behaviour
  def update(dt)
    pos = entity_position
    return unless pos
    hit = Physics.raycast(pos[0], pos[1] + 1.0, pos[2], 0.0, 0.0, 1.0, 6.0)
    if hit
      # hit = [entityId, x, y, z, distance]
      Engine.log("Lookout sieht Entity #{hit[0]} in #{hit[4].round(2)} m")
    end
  rescue => e
    Engine.log("Lookout: #{e}")
  end
end
