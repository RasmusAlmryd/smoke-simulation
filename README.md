
# GPU-Based Smoke Simulation and Rendering
<div align="center"><img src="https://github.com/user-attachments/assets/198b6e67-6445-47dd-9fed-53725c2ed2b1" style="width: 50%"/></div>

This project focused on developing a high-performance smoke simulation and rendering system, leveraging the parallel processing capabilities of the GPU through CUDA and OpenGL. The primary goal was to create a Semi-Lagrangian smoke simulation that could be rendered with consideration for visual effects such as transparency and self-shadowing.

## Inspiration
The simulation follows a Semi-Lagrangian approach that accounts for the advection of air velocity and smoke density. It operates within a volume divided into a grid, where each cell contains velocity and density data, as well as information to determine if it is occupied for object avoidance. The velocity vectors are arranged in a staggered grid to simplify calculations. All simulation steps was executed on the GPU using CUDA [1](https://nccastaff.bmth.ac.uk/jmacey/OldWeb/MastersProjects/MSc13/09/Thesis_Brett_Lambright.pdf), [2](https://physbam.stanford.edu/~fedkiw/papers/stanford2001-01.pdf), [3](https://matthias-research.github.io/pages/tenMinutePhysics/17-fluidSim.pdf).

The rendering strategy is based on NVIDIA's GPU Gems, particularly Chapter 39.5.1 on Volumetric Lighting [1](https://developer.nvidia.com/gpugems/gpugems/part-vi-beyond-triangles/chapter-39-volume-rendering-techniques). This approach enables the development of a rendering pipeline that balances both performance and visual fidelity. The smoke volume is divided into multiple planes, each oriented towards the half-vector between the viewer and the light source. This method ensures that the smoke's appearance remains visually appealing while also allowing for accurate computation of the self-shadowing pass.

## Key Features and Accomplishments
### Features of the Simulation
- Hardware: Nvidia RTX 3070, AMD Ryzen 7 3700X, 32GB RAM
- Simulation Resolution: 80 x 80 x 80
- Number of proxy-geometry planes: 200
- Simulation Ticks Per Seconds: 20
- Frames Per Second: 110 fps
- Main Bottleneck: overdraw, proxy geometry, data communication

### Accomplishments
<div align="center"><img src="https://github.com/user-attachments/assets/1c134a23-5e9c-45a6-b8a4-5056dda6fa31" style="width: 50%"/><p>Smoke demonstration with normal and inverted gravity</p></div>

- **GPU-Accelerated Smoke Simulation**: The project successfully implemented a semi-Lagrangian smoke simulation that runs in parallel on the GPU using CUDA. This approach enabled the simulation to handle complex smoke dynamics in real time.
- **Transparent and Self-Shadowing Rendering**: The rendering process incorporated techniques to account for both transparency and self-shadowing. By dividing the smoke volume into planes oriented towards the camera-light halfway vector, the project ensured that the smoke rendered correctly in various lighting orientations. If the angle between the vectors becomes larger than 90 degrees, the inverse of the view vector is used, and the smoke is rendered in the opposite order. As a result, the rendering pipeline supports both back-to-front and front-to-back rendering.
- **Object Avoidance**: Each cell in the simulation's 3D grid is defined as either solid or air, enabling smoke object avoidance with precision at the level of individual cells.
- **Handles Non-Contained Simulation Volumes**: A significant challenge during development was that the velocity vectors in the simulation became too large for the advection stage to function correctly. Many of the research papers I consulted constrained the smoke within the simulation volume, which naturally limited the velocity vectors. However, I preferred the visual effect of an open simulation, so I made some adjustments. One approach was to increase the number of simulation ticks per second. However, this would have been a temporary fix, increasing performance costs without fully resolving the issue, as the vectors would still eventually become too large. To address this, I instead constrained all velocity vectors to a maximum magnitude, effectively simulating air resistance pushing back on the smoke. Additionally, I ensured that velocity was added only to grid cells containing density.
- **Shadow Map**: One advantage of this rendering approach is that the texture used for smoke self-shadowing can also serve as a shadow map for the rest of the environment. This enhances the overall effect, making it more grounded and realistic.

## Future Considerations and Limitations
While the project achieved its primary goals, future work could focus on implementing the following:
- **Communication Between Simulation and Rendering**: To render the smoke, OpenGL requires access to the volume's density data. Currently, during each render, the CPU copies the density data from the GPU and then stores it back into a 3D texture on the GPU for OpenGL to access. A future improvement would be to keep the density data on the GPU by performing both the simulation and rendering directly on a 3D texture.
- **Vorticity**: The current implementation does not account for vorticity in the smoke. Incorporating vorticity could be a valuable next step for achieving even more realistic smoke effects.
- **Complex Model Collisions**: The simulation can handle complex geometry for object avoidance, but currently, there is no method for generating this data. One potential solution is to render the scene in slices and, for each slice, record the presence of geometry as solid within the volume grid.
- There are opportunities to optimize data storage, especially in the volume grids, by changing data types or using them more efficiently. This could allow for even larger simulations. 
- **Order dependent rendering**: This rendering approach chosen in this project is heavily reliant on rendering each proxy-geometry plane in a particular order, which has a great impact on performance. Another rendering technique like ray-marching maybe should be considered to increase performance.
- **Generating the Proxy Geometry**: The current implementation follows NVIDIA's approach to generating proxy geometry planes. To create each plane, intersections between an infinite plane oriented according to the half-vector and the bounding volume need to be calculated. These intersections are then converted into a plane and triangulated for rendering. Currently, all calculations are performed on the CPU in sequential order, which creates a significant performance bottleneck. There are a couple of immediate solutions to consider. One is to offload some of the calculations to the GPU. Another is to bypass the detailed calculations altogether and simply create larger planes that span the entire volume.


