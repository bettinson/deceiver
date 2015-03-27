#include "load.h"
#include <stdio.h>
#include <GL/glew.h>
#include "lodepng.h"
#include "vi_assert.h"

Loader::Loader(Swapper* s)
	: swapper(s), meshes(), textures(), shaders()
{
}

Asset::ID Loader::mesh(Asset::ID id)
{
	if (id && !meshes[id].refs)
	{
		const char* path = Asset::Model::filenames[id];
		FILE* f = fopen(path, "rb");
		if (!f)
		{
			fprintf(stderr, "Can't open mdl file '%s'\n", path);
			return 0;
		}

		Mesh* mesh = &meshes[id].data;
		new (mesh) Mesh();

		// Read indices
		int count;
		fread(&count, sizeof(int), 1, f);

		// Fill face indices
		mesh->indices.reserve(count);
		mesh->indices.length = count;
		fread(mesh->indices.data, sizeof(int), count, f);

		fread(&count, sizeof(int), 1, f);

		// Fill vertices positions
		mesh->vertices.reserve(count);
		mesh->vertices.length = count;
		fread(mesh->vertices.data, sizeof(Vec3), count, f);

		// Fill vertices texture coordinates
		mesh->uvs.reserve(count);
		mesh->uvs.length = count;
		fread(mesh->uvs.data, sizeof(Vec2), count, f);

		// Fill vertices normals
		mesh->normals.reserve(count);
		mesh->normals.length = count;
		fread(mesh->normals.data, sizeof(Vec3), count, f);

		fclose(f);

		// Physics
		new (&mesh->physics) btTriangleIndexVertexArray(mesh->indices.length / 3, mesh->indices.data, 3 * sizeof(int), mesh->vertices.length, (btScalar*)mesh->vertices.data, sizeof(Vec3));

		// GL
		SyncData* sync = swapper->data();
		sync->op(RenderOp_LoadMesh);
		sync->write<Asset::ID>(&id);
		sync->write<size_t>(&mesh->vertices.length);
		sync->write<Vec3>(mesh->vertices.data, mesh->vertices.length);
		sync->write<Vec2>(mesh->uvs.data, mesh->uvs.length);
		sync->write<Vec3>(mesh->normals.data, mesh->normals.length);
		sync->write<size_t>(&mesh->indices.length);
		sync->write<int>(mesh->indices.data, mesh->indices.length);
	}
	meshes[id].refs++;
	return id;
}

void Loader::unload_mesh(Asset::ID id)
{
	if (id)
	{
		vi_assert(meshes[id].refs > 0);
		if (--meshes[id].refs == 0)
		{
			meshes[id].data.~Mesh();
			SyncData* sync = swapper->data();
			sync->op(RenderOp_UnloadMesh);
			sync->write<Asset::ID>(&id);
		}
	}
}

Asset::ID Loader::texture(Asset::ID id)
{
	if (id && !textures[id].refs)
	{
		const char* path = Asset::Texture::filenames[id];
		unsigned char* buffer;
		unsigned width, height;

		unsigned error = lodepng_decode32_file(&buffer, &width, &height, path);

		if (error)
		{
			fprintf(stderr, "Error loading texture '%s': %s\n", path, lodepng_error_text(error));
			return 0;
		}

		SyncData* sync = swapper->data();
		sync->op(RenderOp_LoadTexture);
		sync->write<Asset::ID>(&id);
		sync->write<unsigned>(&width);
		sync->write<unsigned>(&height);
		sync->write<unsigned char>(buffer, 4 * width * height);
		free(buffer);
	}
	textures[id].refs++;
	return id;
}

void Loader::unload_texture(Asset::ID id)
{
	if (id)
	{
		vi_assert(textures[id].refs > 0);
		if (--textures[id].refs == 0)
		{
			SyncData* sync = swapper->data();
			sync->op(RenderOp_UnloadTexture);
			sync->write<Asset::ID>(&id);
		}
	}
}

Asset::ID Loader::shader(Asset::ID id)
{
	if (id && !shaders[id].refs)
	{
		const char* path = Asset::Shader::filenames[id];

		Array<char> code;
		FILE* f = fopen(path, "r");
		if (!f)
		{
			fprintf(stderr, "Can't open shader source file '%s'", path);
			return 0;
		}

		const size_t chunk_size = 4096;
		int i = 1;
		while (true)
		{
			code.reserve(i * chunk_size + 1); // extra char since this will be a null-terminated string
			size_t read = fread(code.data, sizeof(char), chunk_size, f);
			if (read < chunk_size)
			{
				code.length = ((i - 1) * chunk_size) + read;
				break;
			}
			i++;
		}
		fclose(f);

		SyncData* sync = swapper->data();
		sync->op(RenderOp_LoadShader);
		sync->write<Asset::ID>(&id);
		sync->write<size_t>(&code.length);
		sync->write<char>(code.data, code.length);
	}
	shaders[id].refs++;
	return id;
}

void Loader::unload_shader(Asset::ID id)
{
	if (id)
	{
		vi_assert(shaders[id].refs > 0);
		if (--shaders[id].refs == 0)
		{
			SyncData* sync = swapper->data();
			sync->op(RenderOp_UnloadShader);
			sync->write<Asset::ID>(&id);
		}
	}
}
