# Portrait capture studio (edit mode, no PIE).
import unreal
import os

OUT = r"C:/Game/Arena/JoA_Arena/Saved/Screenshots/Portraits"
os.makedirs(OUT, exist_ok=True)

MESHES = {
    "Knight": ("/Game/_Assets/KayKit/Characters/fbx/SM_Knight.SM_Knight", 0.0),
    "Barbarian": ("/Game/_Assets/KayKit/Characters/fbx/SM_Barbarian.SM_Barbarian", 1.0),
    "Mage": ("/Game/_Assets/KayKit/Characters/fbx/SM_Mage.SM_Mage", 2.0),
    "Ranger": ("/Game/_Assets/KayKit/Characters/fbx/SM_Ranger.SM_Ranger", 3.0),
}
TEAMS = {"FR": 2.0, "EN": 3.0}
STUDIO = unreal.Vector(5000.0, 0.0, 0.0)
CAM = unreal.Vector(4970.0, -380.0, 190.0)
CAM_ROT = unreal.Rotator(-8.0, 95.0, 0.0)

def actors_by_tag(tag):
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    return [a for a in sub.get_all_level_actors() if tag in list(a.tags)]

def delete_studio():
    sub = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
    for a in actors_by_tag("mcp.portrait.studio"):
        sub.destroy_actor(a)
    print("cleared studio")

def spawn_labeled(cls, loc, rot, label):
    a = unreal.EditorLevelLibrary.spawn_actor_from_class(cls, loc, rot)
    a.set_actor_label(label)
    a.tags = ["mcp.portrait.studio", "mcp.placement.ok"]
    return a

def ensure_rt():
    path = "/Game/Materials/Textures/Icons/RT_PortraitCapture"
    rt = unreal.load_asset(path)
    if rt:
        return rt
    factory = unreal.TextureRenderTargetFactoryNew()
    return unreal.AssetToolsHelpers.get_asset_tools().create_asset(
        "RT_PortraitCapture", "/Game/Materials/Textures/Icons",
        unreal.TextureRenderTarget2D, factory)

def main():
    delete_studio()
    mat = unreal.load_asset("/Game/Materials/M_Atlas.M_Atlas")
    if not mat:
        raise RuntimeError("M_Atlas missing")

    bd = spawn_labeled(unreal.StaticMeshActor, unreal.Vector(5000.0, 200.0, 120.0), unreal.Rotator(0,0,0), "Portrait_Backdrop")
    bd.set_actor_scale3d(unreal.Vector(8.0, 0.1, 8.0))
    bd.static_mesh_component.set_static_mesh(unreal.load_asset("/Engine/BasicShapes/Cube.Cube"))

    light = spawn_labeled(unreal.PointLight, unreal.Vector(4900.0, -150.0, 250.0), unreal.Rotator(0,0,0), "Portrait_KeyLight")
    light.point_light_component.set_intensity(12000.0)
    light.point_light_component.set_attenuation_radius(1200.0)
    light.point_light_component.set_cast_shadows(False)

    fill = spawn_labeled(unreal.PointLight, unreal.Vector(5100.0, -100.0, 120.0), unreal.Rotator(0,0,0), "Portrait_FillLight")
    fill.point_light_component.set_intensity(4000.0)
    fill.point_light_component.set_attenuation_radius(800.0)
    fill.point_light_component.set_cast_shadows(False)

    rt = ensure_rt()
    rt.set_editor_property("size_x", 512)
    rt.set_editor_property("size_y", 512)
    rt.set_editor_property("clear_color", unreal.LinearColor(0.05, 0.7, 0.05, 1.0))
    unreal.EditorAssetLibrary.save_asset(rt.get_path_name(), only_if_is_dirty=False)

    capture = spawn_labeled(unreal.SceneCapture2D, CAM, CAM_ROT, "Portrait_Capture")
    cc = capture.capture_component2d
    cc.set_editor_property("texture_target", rt)
    cc.set_editor_property("fov_angle", 32.0)
    cc.set_editor_property("capture_source", unreal.SceneCaptureSource.SCS_FINAL_COLOR_LDR)
    cc.capture_every_frame = False
    cc.capture_on_movement = False

    figure = spawn_labeled(unreal.SkeletalMeshActor, STUDIO, unreal.Rotator(0,0,0), "Portrait_Figure")
    mesh = figure.skeletal_mesh_component
    mesh.set_editor_property("relative_rotation", unreal.Rotator(0.0, -90.0, 0.0))

    world = unreal.EditorLevelLibrary.get_editor_world()
    exported = []
    for unit, (mesh_path, row) in MESHES.items():
        sk = unreal.load_asset(mesh_path)
        if not sk:
            print("missing", mesh_path)
            continue
        mesh.set_skeletal_mesh(sk)
        mesh.set_material(0, mat)
        for team, tile in TEAMS.items():
            mesh.set_custom_primitive_data_float(0, tile)
            mesh.set_custom_primitive_data_float(1, 1.0)
            mesh.set_custom_primitive_data_float(2, row)
            try:
                cc.set_editor_property("primitive_render_mode", unreal.SceneCapturePrimitiveRenderMode.PRM_USE_SHOW_ONLY_LIST)
                cc.show_only_actors = [figure, bd]
            except Exception as e:
                print("showonly", e)
            capture.set_actor_location(CAM, False, True)
            capture.set_actor_rotation(CAM_ROT, False)
            cc.capture_scene()
            name = "Portrait_%s_%s" % (unit, team)
            ok = unreal.RenderingLibrary.export_render_target(world, rt, OUT, name)
            path = os.path.join(OUT, name + ".png")
            exported.append((name, bool(ok), os.path.exists(path)))
            print("shot", name, ok, os.path.exists(path))
    print("DONE", exported)

main()
