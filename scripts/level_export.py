#F exports each selected object into its own file

import bpy
import os
import mathutils
import math

# export to blend file location
basedir = os.path.dirname(bpy.data.filepath)

if not basedir:
    raise Exception("Blend file is not saved")

scene = bpy.context.scene

scenelayersbackup = list(scene.layers)
visible = [0,1,2,3,4]
scene.layers = [l in visible for l in range(20)]

obj_active = scene.objects.active
selection = bpy.context.selected_objects
noncolliding = []
cubemapmesh = []
colliders = []
concave = []
bpy.ops.object.select_all(action='DESELECT')
counter = 0

names = []
for obj in reversed(scene.objects):
    if (obj.layers[10]):
        continue;

    if (getattr(obj, 'type', '') in ["CURVE"]):
        continue

    obj.select = False

    # some exporters only use the active object
    scene.objects.active = obj
    if (obj.layers[1]):
        noncolliding.append(counter)
    if (obj.layers[2]):
        cubemapmesh.append(counter)
    if (obj.layers[3]):
        colliders.append(counter)
    if (obj.layers[4]):
        concave.append(counter)


    name = bpy.path.clean_name(obj.name)
    names.append(name)
    fn = os.path.join(basedir, name)
    prime = obj.matrix_world.copy()
    trf = mathutils.Matrix.Scale(-1, 4, (0.0, 1.0, 0.0))
    #obj.matrix_world = trf * prime
    #scene.update()

    if (obj.name != "Irradiance" and obj.name != "ReflectionProbe"):
        mesh = obj.to_mesh(bpy.context.scene, apply_modifiers=True, settings='RENDER')
        mesh.transform(prime)  # bake the original object transform

        mesh.transform(trf)





        if (prime * trf).determinant() < 0:
            mesh.flip_normals()


        tmp_obj = bpy.data.objects.new("EXPORT_OBJ", mesh)

        bpy.context.scene.objects.link(tmp_obj)
        bpy.context.scene.update()

        tmp_obj.select = True
        bpy.ops.export_scene.obj(filepath=fn + ".obj", use_selection=True,use_triangles=True,axis_up='-Y',axis_forward='-X')
        counter = counter + 1
        tmp_obj.select = False
        bpy.context.scene.objects.unlink(tmp_obj)
        bpy.data.objects.remove(tmp_obj)
        bpy.data.meshes.remove(mesh)
    obj.matrix_world = prime
    #scene.update()

    obj.select = False

    print("written:", fn)


mn = os.path.join(basedir, "manifest")
mani = open(mn + ".txt","w")
mani.write(str(counter)+'\n')
mani.writelines([n+'\n' for n in names])
mani.write(str(len(noncolliding))+'\n')
for id in noncolliding:
    mani.write(str(id)+'\n')
mani.write(str(len(cubemapmesh))+'\n')
for id in cubemapmesh:
    mani.write(str(id)+'\n')
mani.write(str(len(colliders))+'\n')
for id in colliders:
    mani.write(str(id)+'\n')
mani.write(str(len(concave))+'\n')
for id in concave:
    mani.write(str(id)+'\n')


mani.close()

scene.layers = scenelayersbackup

scene.objects.active = obj_active

for obj in selection:
    obj.select = True
