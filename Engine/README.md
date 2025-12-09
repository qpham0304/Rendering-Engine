## Overview
<p>A rendering engine where I experiment different rendering techniques I have learned so far</p>
<p>Everyone is free to reference the project's implementation</p>
<p>However some of the code, mostly shader code, algorithm and math logic are heavily referenced from other sources and papers so please be sure check out their licenses before using at your own risk</p>


## Installation

<p>Cmake is currently not supported but there is plan to support it in the future</P>
<p>For now you can clone the repository using the command:</p>

```
git clone https://github.com/qpham0304/Graphics-Rendering-Engine
 ```

<p>Then call submodule recursively:</p>

```
git submodule update --init --recursive
 ```


## Dependencies
<table>
  <tr>
    <th>Links</th>
  </tr>
  <tr>
    <td>
      <ul>
        <li><a href="https://github.com/nothings/stb">stb texture loader</a></li>
        <li><a href="https://github.com/assimp/assimp">Open Asset Import Library</a></li>
        <li><a href="https://github.com/ocornut/imgui">Dear ImGui</a></li>
        <li><a href="https://github.com/CedricGuillemet/ImGuizmo">Dear ImGuizmo</a></li>
        <li><a href="https://www.glfw.org/">GLFW 3.3</a></li>
        <li><a href="https://glad.dav1d.de/">glad 3.3</a></li>
        <li><a href="https://github.com/g-truc/glm">GLM math library</a></li>
        <li><a href="https://github.com/ThomasMonkman/filewatch">File Watch</a></li>
        <li><a href="https://github.com/ocornut/imgui">Entt</a></li>
      </ul>
    </td>
  </tr>
  
</table>

## Requirement
C++ 17, Window 11 support, Visual Studio 2017, MSVC2017

## Features
<p>Volumetric Light</p>
<p>Atmospheric Scattering (WIP)</p>
<p>God rays</p>
<p>Path Tracing</p>
<p>Cascaded Shadow Map (CSM) (WIP)</p>
<p>Physical Based Rendeing (PBR)</p>
<p>Image Based Lighting (IBL)</p>
<p>Screen Space Ambient Occlusion (SSAO)</p>
<p>Screen Space Reflection (SSR)</p>
<p>Plannar Reflection</p>
<p>Area Light</p>
<p>Instanced Rendering</p>

## Future plan
<p>Add full support for Vulkan</p>
<p>Some Rendering API are tight coupled with OpenGL therefore, some refactoring is needed support Vulkan</p>
<p>Fix inconsistent code styling and convention due to Visual Studio auto format and suggestion</p>
<p>Future support for other platform since the current target Platform is only Desktop Window</p>
<p>Add Cmake Support without over complicating things</p>

## References
<table>
  <tr>
    <th>Techniques</th>
    <th>Reference Links</th>
  </tr>
  <tr>
    <td>Volumetric rendering</td>
    <td>
      <ul>
        <li><a href="https://www.scratchapixel.com/lessons/3d-basic-rendering/volume-rendering-for-developers/intro-volume-rendering.html">scratchapixel Volume Rendering
</a></li>
        <li><a href="https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn">Guerrilla Games's paper
</a></li>
        <li><a href="https://youtu.be/9-HTvoBi0Iw?t=7132">RockStar Games's presentation
</a></li>
        <li><a href="https://blog.42yeah.is/rendering/2023/02/11/clouds.html">Ray marching cloud</li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>Precomputed Atmospheric Scattering</td>
    <td>
      <ul>
        <li><a href="https://sebh.github.io/publications/egsr2020.pdf">Sébastien Hillaire paper</a></li>
        <li><a href="https://www.shadertoy.com/view/slSXRW">Shadertoy's implementation example</a></li>
        <li><a href="https://ebruneton.github.io/precomputed_atmospheric_scattering/">Bruneton's paper</a></li>
        <li><a href="https://www.shadertoy.com/view/wlBXWK">Shadertoy's simpler example</a></li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>God Rays Screen Space</td>
    <td>
      <ul>
        <li><a href="https://medium.com/community-play-3d/god-rays-whats-that-5a67f26aeac2">Simple God ray mask</a></li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>Screen Space Reflection (SSR)</td>
    <td>
      <ul>
        <li><a href="https://lettier.github.io/3d-game-shaders-for-beginners/screen-space-reflection.html">lettier's article</a></li>
        <li><a href="https://sugulee.wordpress.com/2021/01/16/performance-optimizations-for-screen-space-reflections-technique-part-1-linear-tracing-method/">Sugu Lee's article on optimization</a></li>
        <li><a href="https://virtexedge.design/shader-series-basic-screen-space-reflections/">dev log on SSR</a></li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>Image Based Lighting</td>
    <td>
      <ul>
        <li><a href="https://developer.nvidia.com/gpugems/gpugems/part-iii-materials/chapter-19-image-based-lighting">GPU gems</a></li>
        <li><a href="https://learnopengl.com/PBR/IBL/Diffuse-irradiance">Joey de Vries's learn OpenGL</a></li>
        <li><a href="https://learnopengl.com/PBR/IBL/Specular-IBL">Joey de Vries's learn OpenGL</a></li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>Screen Space Ambient Occlusion (SSAO)</td>
    <td>
      <ul>
        <li><a href="https://john-chapman-graphics.blogspot.com/2013/01/ssao-tutorial.html">John Chapman's paper</a></li>
        <li><a href="https://learnopengl.com/Advanced-Lighting/SSAO">Joey de Vries's learn OpenGL</a></li>
        <li><a href="https://mtnphil.wordpress.com/2013/06/26/know-your-ssao-artifacts/">SSAO Artifacts</a></li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>Path Tracing</td>
    <td>
      <ul>
        <li><a href="https://www.youtube.com/playlist?list=PLlrATfBNZ98edc5GshdBtREv5asFW3yXl">The Cherno Ray tracing series</a></li>
        <li><a href="https://github.com/RayTracing">Ray Tracing in One Weekend</a></li>
        <li><a href="https://www.shadertoy.com/view/ldtSR2">Monte Carlos Ray tracing</a></li>
      </ul>
    </td>
  </tr>
  <tr>
    <td>Physically based Bloom</td>
    <td>
      <ul>
        <li><a href="https://learnopengl.com/Guest-Articles/2022/Phys.-Based-Bloom"> Learn OpenGL article</a></li>
        <li><a href="https://www.iryoku.com/next-generation-post-processing-in-call-of-duty-advanced-warfare/">Call of Duty's Siggraph 2014</a></li>
        <li><a href="https://www.youtube.com/watch?v=ml-5OGZC7vE">How Triple AAA Games Render Bloom</a></li>
      </ul>
    </td>
  </tr>
</table>


## Other references
<ul>
  <li><a href="https://www.scratchapixel.com/">https://www.scratchapixel.com/</a></li>
  <li><a href="https://learnopengl.com/">https://learnopengl.com/</a></li>
  <li><a href="https://github.com/TheCherno/Hazel">https://github.com/TheCherno/Hazel</a></li>
  <li><a href="https://lettier.github.io/">https://lettier.github.io/</a></li>
  <li><a href="https://github.com/turanszkij/WickedEngine">https://github.com/turanszkij/WickedEngine</a></li>
</ul>


```
MIT License

Copyright (c)

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```
