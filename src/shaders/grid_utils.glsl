/*
  Compute 3D index of the cell containing given position
*/
inline uint3 getCell3DIndexFromPos(float4 pos)
{
  // Moving particles in [0 - 2 * ABS_WALL_X] to have coords matching with cellIndices
  const float3 posXYZ = clamp(pos.xyz, (float3)(-ABS_WALL_X, -ABS_WALL_Y, -ABS_WALL_Z), (float3)(ABS_WALL_X, ABS_WALL_Y, ABS_WALL_Z))
      + (float3)(ABS_WALL_X, ABS_WALL_Y, ABS_WALL_Z);

  const uint3 cell3DIndex = convert_uint3(floor(posXYZ / GRID_CELL_SIZE_XYZ));

  return cell3DIndex;
}

/*
  Compute 1D index of the cell containing given position
*/
inline uint getCell1DIndexFromPos(float4 pos)
{
  const uint3 cell3DIndex = getCell3DIndexFromPos(pos);

  const uint cell1DIndex = cell3DIndex.x * GRID_RES_Z * GRID_RES_Y
      + cell3DIndex.y * GRID_RES_Z
      + cell3DIndex.z;

  return cell1DIndex;
}