# Stage 6A read-only: hidden/collision state of specific actors, mesh details. Modifies nothing.
import unreal

NAMES = ["S2_Gate_PF4_5", "S2_Scaffold_PF6_7_Panel", "S2_Scaffold_PF6_7_Tarp", "S2_Barricade_PF8", "S2_Prop_TicketBooth",
         "RD_Prop_TicketCounter", "S3C_Closure_Gate_PF4_5", "S3C_Closure_Barricade_PF8", "S3C_Closure_Tarp_PF6_7",
         "S3D_SignClosed_PF4_5", "S3D_SignClosed_PF6_7", "S3D_SignClosed_PF8", "S3C_Track_T9", "S3C_Track_T10",
         "S3B2_StairFinish_PF9_10", "S2_FOB_Deck_B", "S3C_Kiosk_Concourse", "S3B1_FOBSign_BookingOffice",
         "S4D_AdStand_FOB", "S4D_AdStand_PF9", "S3B1_StationClock_PF9_10_W", "S3B1_TrainIndicator_PF9_10_W",
         "S3B1_PlatformSign_PF9_10_W", "S3B1_FOBSign_PF9_10", "S3C_LEDBoard_Concourse", "RD_Concourse_Deck", "GB_Rail_T1_L",
         "S3C_StairWall_PF9_10_N", "S3C_StairBack_PF9_10", "S3C_ScaffoldPole_3930"]
sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
by = {a.get_actor_label(): a for a in sub.get_all_level_actors()}
for n in NAMES:
    a = by.get(n)
    if not a:
        unreal.log("P9PY: %s MISSING" % n)
        continue
    hid = a.get_editor_property("hidden")
    coll = a.get_actor_enable_collision()
    s = []
    for c in a.get_components_by_class(unreal.StaticMeshComponent):
        m = c.static_mesh
        info = ""
        if m:
            nv = unreal.EditorStaticMeshLibrary.get_number_verts(m, 0) if hasattr(unreal, "EditorStaticMeshLibrary") else -1
            info = "%s v=%s slots=%s" % (m.get_name(), nv, [str(x.material_slot_name) for x in m.static_materials])
        s.append("%s[%s vis=%s coll=%s mats=%s]" % (c.get_name(), info, c.is_visible(), str(c.get_collision_enabled()).split(".")[-1],
                 [x.get_name() if x else None for x in c.get_materials()]))
    tags = [str(t) for t in a.tags]
    unreal.log("P9PY: %s hidden=%s coll=%s tags=%s %s" % (n, hid, coll, tags, " ".join(s)))
