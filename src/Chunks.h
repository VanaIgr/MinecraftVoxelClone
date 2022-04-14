#pragma once

#include<vector>
#include<unordered_map>
#include<stdint.h>
#include<array>
#include<tuple>
#include<utility>

#include"Vector.h"

namespace chunk {
	//all the constans are copied into main.shader
	static constexpr int const blocksInChunkDimAsPow2 = 4;
	static constexpr int const blocksInChunkDim = 1 << blocksInChunkDimAsPow2;
	static constexpr int const blocksInChunkCount = blocksInChunkDim*blocksInChunkDim*blocksInChunkDim;
	
	static constexpr int const cubesInBlockDimAsPow2 = 1;
	static constexpr int const cubesInBlockDim = 1 << cubesInBlockDimAsPow2;
	static constexpr int const cubesInBlockCount = cubesInBlockDim*cubesInBlockDim*cubesInBlockDim;
	
	static constexpr int const cubesInChunkDimAsPow2 = cubesInBlockDimAsPow2 + blocksInChunkDimAsPow2;
	static constexpr int const cubesInChunkDim = 1 << cubesInChunkDimAsPow2;
	static constexpr int const cubesInChunkCount = cubesInChunkDim*cubesInChunkDim*cubesInChunkDim;
	
	static constexpr bool checkBlockCoordInChunkValid(vec3i const coord) {
		return coord.inMMX(vec3i{0}, vec3i{blocksInChunkDim}).all();
	}
	static constexpr bool checkBlockIndexInChunkValid(uint16_t const index) {
		return index < blocksInChunkCount;
	}
	
	inline static constexpr int16_t blockIndex(vec3i const coord) {
		assert(checkBlockCoordInChunkValid(coord));
		return coord.x + coord.y*chunk::blocksInChunkDim + coord.z*chunk::blocksInChunkDim*chunk::blocksInChunkDim;
	}
	inline static constexpr vec3i indexBlock(int16_t index) {
		assert(checkBlockIndexInChunkValid(index));
		return vec3i{ index % chunk::blocksInChunkDim, (index / chunk::blocksInChunkDim) % chunk::blocksInChunkDim, (index / chunk::blocksInChunkDim / chunk::blocksInChunkDim) };
	}
	
	static constexpr bool checkCubeCoordInChunkValid(vec3i const coord) {
		return coord.inMMX(vec3i{0}, vec3i{cubesInChunkDim}).all();
	}
	static constexpr bool checkCubeIndexInChunkValid(uint32_t const index) {
		return index < cubesInChunkCount;
	}
	
	static vec3i cubeCoordInChunk_(uint32_t const index) { ///used in main.shader
		assert(checkCubeIndexInChunkValid(index));
		return vec3i( index % chunk::cubesInChunkDim, (index / chunk::cubesInChunkDim) % chunk::cubesInChunkDim, (index / chunk::cubesInChunkDim / chunk::cubesInChunkDim) );
	}
	static uint32_t cubeIndexInChunk(vec3i const coord) { ///used in main.shader
		assert(checkCubeCoordInChunkValid(coord));
		return coord.x + coord.y*chunk::cubesInChunkDim + coord.z*chunk::cubesInChunkDim*chunk::cubesInChunkDim;
	}
	
	template<typename Chunks>
	struct Chunk_{
	private:
		Chunks *chunks_;
		int chunk_index;
	public:
		Chunk_() = default;
		
		Chunk_(Chunks &chunks, int const chunkIndex) : chunks_{ &chunks }, chunk_index{ chunkIndex } {}
		
		bool operator!=(Chunk_<Chunks> const other) const {
			return chunks_ != other.chunks_ || chunk_index != other.chunk_index;
		}
			
		auto &chunks() { return *chunks_; }	
		auto const &chunks() const { return *chunks_; }

		auto &chunkIndex() { return chunk_index; }	
		auto const &chunkIndex() const { return chunk_index; }	
			
		#define gs(name, accessor) decltype(auto) name () { return chunks(). accessor [chunk_index]; } decltype(auto) name () const { return chunks(). accessor [chunk_index]; }	
			gs(position, chunksPos)
			gs(aabb, chunksAABB)
			gs(gpuStatus, gpuChunksStatus)
			gs(modified, modified) //is this safe? (gpuPresent returns rvalue reference)
			gs(data, chunksData)
			gs(neighbours, chunksNeighbours)
			gs(ao, chunksAO)
			gs(lighting, chunksLighting)
		#undef gs
	};
	
	struct Chunks;
	using Chunk = Chunk_<Chunks>;

	
	struct Block { //used in main.shader
		static_assert(chunk::cubesInBlockCount <= 8, "cubes state must fit into 8 bits");
		
		static constexpr bool checkCubeCoordValid(vec3i const coord) {
			return coord.inMMX(vec3i{0}, vec3i{chunk::cubesInBlockDim}).all();
		}
		
		static constexpr bool checkCubeIndexValid(uint8_t const index) {
			return index < chunk::cubesInBlockCount;
		}
		
		static constexpr uint8_t cubePosIndex(vec3i const pos) {
			assert(checkCubeCoordValid(pos));
			return pos.x + (pos.y << chunk::cubesInBlockDimAsPow2) + (pos.z << (chunk::cubesInBlockDimAsPow2*2));
		}
		
		static constexpr vec3i cubeIndexPos(uint8_t const index) {
			assert(checkCubeIndexValid(index));
			return vec3i(
				 index                                      % chunk::cubesInBlockDim, 
				(index >>  chunk::cubesInBlockDimAsPow2   ) % chunk::cubesInBlockDim, 
				(index >> (chunk::cubesInBlockDimAsPow2*2)) % chunk::cubesInBlockDim
			);
		}
		
		static constexpr uint8_t blockCubeMask(uint8_t const index) {
			assert(checkCubeIndexValid(index));
			return 1 << index;
		}
		
		static constexpr uint8_t blockCubeMask(vec3i const coord) {
			const auto index{ cubePosIndex(coord) };
			return 1 << index;
		}
		
		static constexpr bool blockCube(uint8_t const cubes, uint8_t const index) {
			assert(checkCubeIndexValid(index));
			return (cubes >> index) & 1;
		}
		
		static constexpr bool blockCube(uint8_t const cubes, vec3i const coord) {
			const auto index{ cubePosIndex(coord) };
			return (cubes >> index) & 1;
		}
		
		static constexpr Block fullBlock(uint16_t const id) { return Block(id, 0b1111'1111); }
		static constexpr Block emptyBlock() { return Block(0, 0); }
	private:
		uint32_t data_;
	public:
		Block() = default;
		explicit constexpr Block(uint32_t const data__) : data_{ data__ } {}
		constexpr Block(uint16_t const id, uint8_t cubes) : data_{ uint32_t(id) | (uint32_t(cubes) << 24) } {
			if(id == 0 || cubes == 0) data_ = 0;
		}
		
		constexpr uint16_t id() const { return uint16_t(data_ & ((1 << 16) - 1)); }
		constexpr bool cube(vec3i const coord) const { return blockCube(cubes(), coord); }
		constexpr bool cube(uint8_t const index) const { return blockCube(cubes(), index); }
		constexpr bool empty() const { return data_ == 0; }
		
		uint8_t cubes() const { return uint8_t(data_ >> 24); }
		uint32_t data() const { return data_; }
	};
	
	
	struct AABB { //used in main.shader
		static constexpr int64_t cd = chunk::blocksInChunkDim-1;
		static_assert( cd*cd*cd * cd*cd*cd < (1ll << 32), "two block indices must fit into 32 bits" );
	private:
		uint32_t data;
	public:
		AABB() = default;
		//AABB(int16_t const b1, int16_t const b2) : data{ uint32_t(uint16_t(b1)) | (uint32_t(uint16_t(b2)) << 16) } {}
		AABB(vec3i const start, vec3i const end) : data{ 
			uint32_t(uint16_t(blockIndex(start)))
			| (uint32_t(uint16_t(blockIndex(end))) << 16) 
		} {
			assert(checkBlockCoordInChunkValid(start) && checkBlockCoordInChunkValid(end));
		}
		
		constexpr uint32_t getData() const { return data; } //used in vectex.shader, debug program
		constexpr vec3i start() const { return indexBlock(int16_t(data&0xffff)); } //used in vectex.shader, debug program
		constexpr vec3i end() const { return indexBlock(int16_t(data>>16)); } //used in main vertex shader //used in vectex.shader, debug program
		constexpr vec3i onePastEnd() const { return end() + 1; } //used in main vertex shader //used in vectex.shader, debug program
		constexpr bool empty() const { return (end() < start()).any(); };
	};
	
	
	struct OptionalChunkIndex { //used in main.shader
	// -(chunkIndex) - 1	
	private: int n;
	public:
		OptionalChunkIndex() = default;
		OptionalChunkIndex(int chunkIndex) : n{ -chunkIndex - 1 } {}
		explicit operator int() const { return get(); }
		
		operator bool() const { return is(); }
		bool is() const {
			return n != 0;
		}
		
		int32_t get() const { //return -1 if invalid
			return int32_t(int64_t(n + 1) * -1); //-n - 1 is UB if n is integer min?
		}
	};
	
	
	struct Neighbours {
		static constexpr int neighboursCount{ 6 };
		
		static constexpr bool checkIndexValid(uint8_t const index) {
			return index < neighboursCount;
		}
		
		static constexpr bool checkDirValid(vec3i const dir) {
			return vec3i(dir.notEqual(0)).dot(1) == 1;
		}
		
		//used in main.shader
			static constexpr vec3i indexAsDir(uint8_t neighbourIndex) {
				assert(checkIndexValid(neighbourIndex));
				vec3i const dirs[] = { vec3i{-1,0,0},vec3i{1,0,0},vec3i{0,-1,0},vec3i{0,1,0},vec3i{0,0,-1},vec3i{0,0,1} };
				return dirs[neighbourIndex];
			}
			static constexpr uint8_t dirAsIndex(vec3i dir) {
				assert(checkDirValid(dir));
				auto const result{ (dir.x+1)/2 + (dir.y+1)/2+abs(dir.y*2) + (dir.z+1)/2+abs(dir.z*4) };
				if(indexAsDir(result) != dir) {
					std::cerr << "err: " << dir << ' ' << result << '\n';
					assert(false);
				}
				return result; 
			}
			static constexpr uint8_t mirror(uint8_t index) {
				return dirAsIndex( -indexAsDir(index) );
			}
			static constexpr uint8_t mirror(vec3i const dir) {
				return dirAsIndex( -dir );
			}
		
		std::array<OptionalChunkIndex, neighboursCount> n;
		
		OptionalChunkIndex &operator[](uint8_t index) { assert(checkIndexValid(index)); return n[index]; }
		OptionalChunkIndex const &operator[](uint8_t index) const { assert(checkIndexValid(index)); return n[index]; }
		
		OptionalChunkIndex &operator[](vec3i dir) { return n[dirAsIndex(dir)]; }
		OptionalChunkIndex const &operator[](vec3i dir) const { return n[dirAsIndex(dir)]; }
	};
	
	
	struct GPUChunkStatus {
	private:
		uint8_t status : 2;
		uint8_t flags  : 1;
	public:
		GPUChunkStatus() = default;
		
		bool isFullyLoaded() const { return (status & 1) != 0; }
		void markFullyLoaded() { reset(); status = 1; }
		
		bool isStubLoaded() const { return (status & 2) != 0; }
		void markStubLoaded() { reset(); status = 2; }
		
		bool needsUpdate () const { return (flags & 1) == 0; }
		void setNeedsUpdate()   { flags &= ~1; }
		void resetNeedsUpdate() { flags |=  1; }
		
		void reset() { status = 0; }
	};
	
	
	struct ChunkAO {
		static constexpr int size = chunk::cubesInChunkCount;
		
		static vec3i dirsForIndex(const int index) { //used in main.shader
			assert(index < 8); //at most 8 cubes share 1 vertex
			const int x = int((index % 2)       != 0); //0, 2, 4, 6 - 0; 1, 3, 5, 7 - 1
			const int y = int(((index / 2) % 2) != 0); //0, 1, 4, 5 - 0; 2, 3, 6, 7 - 1
			const int z = int((index / 4)       != 0); //0, 1, 2, 3 - 0; 4, 5, 6, 7 - 1
			return vec3i{x,y,z} * 2 - 1;
		}
	private:
		std::array<uint8_t, size> vertsBlocks;
	public:
		//ChunkAO() = default;
		
		uint8_t       &operator[](int const index)       { return vertsBlocks[index]; }
		uint8_t const &operator[](int const index) const { return vertsBlocks[index]; }
		
		void reset() { vertsBlocks.fill(0); }
	};

	
	struct ChunkLighting {
		static constexpr int size = chunk::cubesInChunkCount;
		
		static constexpr int dirsCount{ 6 };
		
		static constexpr bool checkIndexValid(uint8_t const index) {
			return index < dirsCount;
		}
		
		static constexpr bool checkDirValid(vec3i const dir) {
			return vec3i(dir.notEqual(0)).dot(1) == 1;
		}
		
		static constexpr vec3i indexAsDir(uint8_t neighbourIndex) {
			assert(checkIndexValid(neighbourIndex));
			vec3i const dirs[] = { vec3i{-1,0,0},vec3i{1,0,0},vec3i{0,-1,0},vec3i{0,1,0},vec3i{0,0,-1},vec3i{0,0,1} };
			return dirs[neighbourIndex];
		}
		static constexpr uint8_t dirAsIndex(vec3i dir) {
			assert(checkDirValid(dir));
			auto const result{ (dir.x+1)/2 + (dir.y+1)/2+abs(dir.y*2) + (dir.z+1)/2+abs(dir.z*4) };
			if(indexAsDir(result) != dir) {
				std::cerr << "err: " << dir << ' ' << result << '\n';
				assert(false);
			}
			return result; 
		}
		static constexpr uint8_t lightForDirIndex(uint8_t const light, uint8_t const index) {
			return index == 2 /*(0, -1, 0)*/  ? light : uint8_t(misc::max(int(light) - 1, 0));
		}
			
		static constexpr uint8_t lightForDir(uint8_t const light, vec3i const dir) {
			return lightForDirIndex(light, dirAsIndex(dir));
		}
	private:
		std::array<uint8_t, size> lighting;
	public:
		ChunkLighting() = default;
		ChunkLighting(uint8_t const val) {
			fill(val);
		}
		
		uint8_t       &operator[](int const cubeIndex)       { return lighting[cubeIndex]; }
		uint8_t const &operator[](int const cubeIndex) const { return lighting[cubeIndex]; }		
		
		uint8_t       &operator[](vec3i const cubeCoord)       { return lighting[cubeIndexInChunk(cubeCoord)]; }
		uint8_t const &operator[](vec3i const cubeCoord) const { return lighting[cubeIndexInChunk(cubeCoord)]; }
		
		void fill(uint8_t const val) { lighting.fill(val); }
		void reset() { fill(0); }
	};
	
	
	using ChunkData = std::array<Block, chunk::blocksInChunkCount>;
	
	
	struct Chunks {	
	private:
		std::vector<int> vacant{};
		std::vector<int> used_{};
		
		struct PosHash { 
			constexpr inline std::size_t operator()(vec3i const &it) const noexcept { 
				return  (std::hash<int32_t>{}(it.x) ^ (std::hash<int32_t>{}(it.x) << 1)) ^ (std::hash<int32_t>{}(it.z) << 1);
			} 
		};
	public:
		std::vector<int> used{};
		
		std::vector<vec3i> chunksPos{};
		std::vector<AABB> chunksAABB{};
		std::vector<GPUChunkStatus> gpuChunksStatus{};
		std::vector<bool> modified{};
		std::vector<ChunkData> chunksData{};
		std::vector<ChunkAO> chunksAO;
		std::vector<ChunkLighting> chunksLighting;
		std::vector<Neighbours> chunksNeighbours{};
		std::unordered_map<vec3i, int, PosHash> chunksIndex_position{};
		
		//returns used[] position 
		//all the chunk data is not initialised
		inline int reserve() {
			int index;
			int usedSize = used.size();
	
			if(!vacant.empty()) { 
				index = vacant[vacant.size()-1];
				vacant.pop_back();
			}
			else { //TODO: avoid zero-init
				index = usedSize;
				chunksPos.resize(index+1);
				chunksAABB.resize(index+1);
				gpuChunksStatus.resize(index+1);
				modified.resize(index+1);
				chunksData.resize(index+1);
				chunksAO.resize(index+1);
				chunksLighting.resize(index+1);
				chunksNeighbours.resize(index+1);
			}
			used.push_back(index);
			
			return usedSize;
		}
	
		inline void recycle(int const index) {
			auto chunkIndex = used[index];
			chunksNeighbours[chunkIndex] = Neighbours();
			used.erase(used.begin()+index);
			vacant.push_back(chunkIndex);
		}
	
		
		template<typename Action>
		inline void forEachUsed(Action &&action) const {
			for(auto const chunkIndex : used) {
				action(chunkIndex);
			}
		}
		
		template<typename Predicate, typename Free>
		inline void filterUsed(Predicate&& keep, Free &&free) {
			auto const sz = used.size();
			
			for(size_t i = 0; i < sz; ++i) {
				auto const &chunkIndex = used[i];
				if(keep(chunkIndex)) used_.push_back(chunkIndex);
				else { free(chunkIndex); vacant.push_back(chunkIndex); }
			}
			
			used.clear();
			used.swap(used_);
		}
		
		inline Chunk operator[](int chunkIndex) {
			return Chunk{ *this, chunkIndex };
		}
	};
	
	
	struct Move_to_neighbour_Chunk {
	private:
		Chunk chunk;
		bool valid;
	public:
		Move_to_neighbour_Chunk() = delete;
		
		Move_to_neighbour_Chunk(Chunks &chunks) : chunk{chunks[0]}, valid{ true } {}/*index 0 may be out of bounds but we need to keep Chunks&*/
		Move_to_neighbour_Chunk(Chunk &src) : chunk{src}, valid{ true } {}
			
		Move_to_neighbour_Chunk(Chunks &chunks, vec3i const chunkCoord) {
			auto const chunkIndexP{ chunks.chunksIndex_position.find(chunkCoord) };
			
			valid = chunkIndexP != chunks.chunksIndex_position.end();
			
			if(valid) chunk = chunks[chunkIndexP->second];
			else chunk = chunks[0]; //index 0 may be out of bounds but we need to keep Chunks&
		}
		
		OptionalChunkIndex optChunk() const {
			if(valid) return OptionalChunkIndex{ chunk.chunkIndex() };
			return {};
		}
		
		OptionalChunkIndex move(vec3i const otherChunk) {
			if(valid && Neighbours::checkDirValid(otherChunk - chunk.position())) return offset(otherChunk - chunk.position());
			if(valid && otherChunk == chunk.position()) return optChunk();
			*this = Move_to_neighbour_Chunk(chunk.chunks(), otherChunk);
			return optChunk();
		}
		
		OptionalChunkIndex moveToNeighbour(vec3i const neighbour) {
			if(!valid) return {};
			return offset(neighbour - chunk.position());
		}
			
		OptionalChunkIndex offset(vec3i const dir) {
			if(!Neighbours::checkDirValid(dir)) {
				std::cerr << "dir is invalid:" << dir << '\n';
				assert(false);
				exit(-1);
			}
			if(!valid) return {};
			if(dir == 0) return { chunk.chunkIndex() };
			auto const optChunkIndex{ chunk.neighbours()[dir] };
			valid = optChunkIndex.is();
			if(valid) chunk = chunk.chunks()[optChunkIndex.get()];
			return optChunkIndex;
		}
		
		bool is() const { return valid; }
	};
}