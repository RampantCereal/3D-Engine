# 3D Engine
 C++ 3D engine with modern pipeline features
 
 
Eric Estévez Jiménez - 193073

Todo lo relevante a la entrega final esta implenentado en la pipeline de Deferred.

Los controles son los que estaban por defecto + la opción de cambiar de pipeline con la tecla P.

En la escena hay dos coches, la casa del prefab brutalism, una spotlight verde encima de uno de los coches y una point light roja entre ambos.

SSAO+ esta implementado y se puede activar/desactivar y mostrar el buffer de SSAO+ desde imGui.

La irradiancia esta implentada con probes, imGui permite calular la irradiancia, ver la grid de probes (una vez calculada) y activar y desactivar la irradiancia en el color final.

Los relfejos estan implementados con probes, hay tres probes en la escena, una entre los dos coches, otra a la izquierda del todo y otra a la derecha del todo.
Los relfejos se aplican a las partes mas metalicas del coche, y cambian entre la probe mas cercana y el skybox en funcion de la distancia.
Desde imGui se pueden calcular los relfejos de las probes, activar/desactivar los relfejos y mostrar las probes.

Se puede activar/desactivar la directional volumetrica desde imGui, esta utiliza una textura de noise para quitar el banding.

Por ultimo, hay un decal implementado que se puede activar/desactivar desde imGui tambien.

![Decal](https://user-images.githubusercontent.com/12513993/177052610-5724a3c9-7295-4724-b436-7974faa341ed.PNG)
