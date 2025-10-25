/*
  Compute 3D index of the cell containing given position
*/
uvec3 getCell3DIndexFromPos(vec3 pos)
{
  // Moving particles in [0 - 2 * ABS_WALL_X] to have coords matching with cellIndices
  const vec3 normalizedPos = clamp(pos, MIN_BOUNDS, MAX_BOUNDS) + MAX_BOUNDS;

  // Clamping to ensure we rest within the grid boundaries even for outer grid vertices
  const uvec3 cell3DIndex = clamp(uvec3(floor(normalizedPos.xyz / GRID_CELL_SIZE_XYZ)),
      uvec3(0, 0, 0), uvec3(GRID_RES_X - 1, GRID_RES_Y - 1, GRID_RES_Z - 1));

  return cell3DIndex;
}

/*
  Compute 1D index of the cell containing given position
*/
uint getCell1DIndexFromPos(vec3 pos)
{
  const uvec3 cell3DIndex = getCell3DIndexFromPos(pos);

  const uint cell1DIndex = cell3DIndex.x * GRID_RES_Z * GRID_RES_Y
      + cell3DIndex.y * GRID_RES_Z
      + cell3DIndex.z;

  return cell1DIndex;
}