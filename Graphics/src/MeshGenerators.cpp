#include "stdafx.h"
#include "MeshGenerators.hpp"

namespace Graphics
{
	Mesh MeshGenerators::Quad(OpenGL* gl, Vector2 pos, Vector2 size /*= Vector2(1,1)*/)
	{
		Vector<SimpleVertex> verts =
		{
			{ { 0.0f,  size.y, 0.0f }, { 0.0f, 0.0f } },
			{ { size.x, 0.0f,  0.0f }, { 1.0f, 1.0f } },
			{ { size.x, size.y, 0.0f }, { 1.0f, 0.0f } },

			{ { 0.0f,  size.y, 0.0f }, { 0.0f, 0.0f } },
			{ { 0.0f,  0.0f,  0.0f }, { 0.0f, 1.0f } },
			{ { size.x, 0.0f,  0.0f }, { 1.0f, 1.0f } },
		};

		for(auto& v : verts)
		{
			v.pos += pos;
		}

		Mesh mesh = MeshRes::Create(gl);
		mesh->SetData(verts);
		mesh->SetPrimitiveType(PrimitiveType::TriangleList);
		return mesh;
	}

	void MeshGenerators::GenerateSimpleXYQuad(Rect3D r, Rect uv, Vector<SimpleVertex>& out)
	{
		Vector<MeshGenerators::SimpleVertex> verts =
		{
			{ { r.Left(),  r.Top(),     0.0f },{ uv.Left(), uv.Top() } },
			{ { r.Right(), r.Bottom(),  0.0f },{ uv.Right(), uv.Bottom() } },
			{ { r.Right(), r.Top(),     0.0f },{ uv.Right(), uv.Top() } },

			{ { r.Left(),  r.Top(),     0.0f },{ uv.Left(), uv.Top() } },
			{ { r.Left(),  r.Bottom(),  0.0f },{ uv.Left(), uv.Bottom() } },
			{ { r.Right(), r.Bottom(),  0.0f },{ uv.Right(), uv.Bottom() } },
		};
		for(auto& v : verts)
			out.Add(v);
	}
	void MeshGenerators::GenerateSimpleXZQuad(Rect3D r, Rect uv, Vector<MeshGenerators::SimpleVertex>& out)
	{
		Vector<MeshGenerators::SimpleVertex> verts =
		{
			{ { r.Left(),  0.0f, r.Top(),    },{ uv.Left(), uv.Top() } },
			{ { r.Right(), 0.0f, r.Bottom(), },{ uv.Right(), uv.Bottom() } },
			{ { r.Right(), 0.0f, r.Top(),    },{ uv.Right(), uv.Top() } },
						   
			{ { r.Left(),  0.0f, r.Top(),    },{ uv.Left(), uv.Top() } },
			{ { r.Left(),  0.0f, r.Bottom(), },{ uv.Left(), uv.Bottom() } },
			{ { r.Right(), 0.0f, r.Bottom(), },{ uv.Right(), uv.Bottom() } },
		};
		for(auto& v : verts)
			out.Add(v);
	}

	//TODO(skade)
	void MeshGenerators::GenerateSubdividedTrack(Rect3D r, Rect uv, uint32_t count, Vector<SimpleVertex>& out)
	{
		Vector<MeshGenerators::SimpleVertex> vL;
		
		vL.emplace_back(Vector3(r.Left(), r.Top(), 0.0f),Vector2(uv.Left(), uv.Top()));
		vL.emplace_back(Vector3(r.Right(), r.Top(), 0.0f),Vector2(uv.Right(), uv.Top()));
		
		uint32_t amount = count;
		
		for (uint32_t i = 1; i <= amount; ++i) {
			float l = (float) i/amount;
			float posSize = r.size.y+r.pos.y;
			l = 1.f-(posSize*l)/r.size.y;
			float rv = r.Top()*l+(1.f-l)*r.Bottom();
			float uvv = uv.Top()*l+(1.f-l)*uv.Bottom();
			
			vL.emplace_back(Vector3(r.Left(), rv, 0.0f),Vector2(uv.Left(), uvv));
			vL.emplace_back(Vector3(r.Right(),rv, 0.0f),Vector2(uv.Right(),uvv));
		}

		vL.emplace_back(Vector3(r.Left(), r.Bottom(), 0.0f),Vector2(uv.Left(), uv.Bottom()));
		vL.emplace_back(Vector3(r.Right(),r.Bottom(), 0.0f),Vector2(uv.Right(),uv.Bottom()));
		
		for(auto& v : vL)
			out.Add(v);
	}

	Vector<MeshGenerators::SimpleVertex> MeshGenerators::Triangulate(Vector<MeshGenerators::SimpleVertex> verts) {
		Vector<MeshGenerators::SimpleVertex> ret;
		for (uint32_t i = 0; i < verts.size()-2; ++i) {
			if (i % 2 == 0) {
				ret.push_back(verts[i]);
				ret.push_back(verts[i+2]);
				ret.push_back(verts[i+1]);
			} else {
				ret.push_back(verts[i]);
				ret.push_back(verts[i+1]);
				ret.push_back(verts[i+2]);
			}
		}
		return ret;
	}
}
