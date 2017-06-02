#include"AssimpLoader.h"
#include<assimp/postprocess.h>
#include<assimp/cimport.h>
#include<hgl/FileSystem.h>

#define LOG_BR	LOG_INFO(OS_TEXT("------------------------------------------------------------"));

#define aisgl_min(x,y) (x<y?x:y)
#define aisgl_max(x,y) (y>x?y:x)

void set_float4(float f[4], float a, float b, float c, float d)
{
	f[0] = a;
	f[1] = b;
	f[2] = c;
	f[3] = d;
}

void color4_to_float4(const aiColor4D *c, float f[4])
{
	f[0] = c->r;
	f[1] = c->g;
	f[2] = c->b;
	f[3] = c->a;
}

inline Matrix4f matrix_convert(const aiMatrix4x4 &m)
{
	Matrix4f result;

	memcpy(&result,&m,16*sizeof(float));

	return result;
}

void AssimpLoader::get_bounding_box_for_node (const aiNode* nd, 
												aiVector3D* min, 
												aiVector3D* max, 
												aiMatrix4x4* trafo)
{
	aiMatrix4x4 prev;
	unsigned int n = 0, t;

	prev = *trafo;
	aiMultiplyMatrix4(trafo,&nd->mTransformation);

	for (; n < nd->mNumMeshes; ++n) 
	{
		const aiMesh* mesh = scene->mMeshes[nd->mMeshes[n]];

		for (t = 0; t < mesh->mNumVertices; ++t) 
		{
			aiVector3D tmp = mesh->mVertices[t];
			aiTransformVecByMatrix4(&tmp,trafo);

			min->x = aisgl_min(min->x,tmp.x);
			min->y = aisgl_min(min->y,tmp.y);
			min->z = aisgl_min(min->z,tmp.z);

			max->x = aisgl_max(max->x,tmp.x);
			max->y = aisgl_max(max->y,tmp.y);
			max->z = aisgl_max(max->z,tmp.z);
		}
	}

	for (n = 0; n < nd->mNumChildren; ++n)
		get_bounding_box_for_node(nd->mChildren[n],min,max,trafo);

	*trafo = prev;
}

void AssimpLoader::get_bounding_box (aiVector3D* min, aiVector3D* max)
{
	aiMatrix4x4 trafo;

	aiIdentityMatrix4(&trafo);

	min->x = min->y = min->z =  1e10f;
	max->x = max->y = max->z = -1e10f;

	get_bounding_box_for_node(scene->mRootNode,min,max,&trafo);
}

AssimpLoader::AssimpLoader()
{
	scene=0;
}

AssimpLoader::~AssimpLoader()
{
	if(scene)
		aiReleaseImport(scene);
}

void AssimpLoader::SaveFile(void *data,uint size,const OSString &ext_name)
{
	if(!data||size<=0)return;

	OSString filename=main_filename+OS_TEXT(".")+ext_name;

	if(filesystem::SaveMemoryToFile(filename,data,size)==size)
	{
		LOG_INFO(OS_TEXT("SaveFile: ")+filename+OS_TEXT(" , size: ")+OSString(size));

		total_file_bytes+=size;
	}
}

#pragma pack(push,1)
struct MaterialStruct
{
	UTF8String filename;

	uint8 tm=0;
	uint32 uvindex=0;
	float blend=0;
	uint32 op=0;
	uint32 wrap_mode[2]={0,0};

	Color4f diffuse;
	Color4f specular;
	Color4f ambient;
	Color4f emission;

	float shininess=0;

	bool wireframe=false;
	bool two_sided=false;
};
#pragma pack(pop)

void OutFloat3(const OSString &front,const Color4f &c)
{
	LOG_INFO(front+OS_TEXT(": ")+OSString(c.r)
					+OS_TEXT(",")+OSString(c.g)
					+OS_TEXT(",")+OSString(c.b));
}

void OutMaterial(const MaterialStruct &ms)
{
	LOG_INFO(U8_TEXT("Filename: ")+ms.filename);

	LOG_INFO(OS_TEXT("Mapping: ")+OSString(ms.tm));
	LOG_INFO(OS_TEXT("uvindex: ")+OSString(ms.uvindex));
	LOG_INFO(OS_TEXT("blend: ")+OSString(ms.blend));
	LOG_INFO(OS_TEXT("op: ")+OSString(ms.op));
	LOG_INFO(OS_TEXT("wrap_mode: ")+OSString(ms.wrap_mode[0])+OS_TEXT(",")+OSString(ms.wrap_mode[1]));

	OutFloat3(OSString(OS_TEXT("diffuse")),ms.diffuse);
	OutFloat3(OSString(OS_TEXT("specular")),ms.specular);
	OutFloat3(OSString(OS_TEXT("ambient")),ms.ambient);
	OutFloat3(OSString(OS_TEXT("emission")),ms.emission);
	
	LOG_INFO(OS_TEXT("shininess: ")+OSString(ms.shininess));

	LOG_INFO(OS_TEXT("wireframe: ")+OSString(ms.wireframe?OS_TEXT("true"):OS_TEXT("false")));
	LOG_INFO(OS_TEXT("two_sided: ")+OSString(ms.two_sided?OS_TEXT("true"):OS_TEXT("false")));
}

void AssimpLoader::LoadMaterial()
{
	if(!scene->HasMaterials())return;

	int ret1, ret2;
	float c[4];
	aiColor4D diffuse;
	aiColor4D specular;
	aiColor4D ambient;
	aiColor4D emission;
	float shininess;
	float strength;
	int two_sided;
	int wireframe;
	unsigned int max;	// changed: to unsigned

	LOG_BR;
	LOG_INFO(OS_TEXT("Material Count ")+OSString(scene->mNumMaterials));
	LOG_BR;
	
	aiString filename;
	aiTextureMapping tm;
	uint uvindex=0;
	float blend;
	aiTextureOp op;
	aiTextureMapMode wrap_mode[2];

	for(unsigned int m=0;m<scene->mNumMaterials;m++)
	{
		int tex_index=0;
		aiReturn found=AI_SUCCESS;

		aiMaterial *mtl=scene->mMaterials[m];

		found=mtl->GetTexture(aiTextureType_DIFFUSE,tex_index,&filename,&tm,&uvindex,&blend,&op,wrap_mode);
		
		MaterialStruct ms;

		{
			char *fn=strrchr(filename.data,'\\');

			if(!fn)
				fn=strrchr(filename.data,'/');
			else
				++fn;

			if(fn)
				++fn;
			else
				fn=filename.data;

			ms.filename=fn;
		}

		ms.tm=tm;
		ms.uvindex=uvindex;
		ms.blend=blend;
		ms.op=op;

		ms.wrap_mode[0]=wrap_mode[0];
		ms.wrap_mode[1]=wrap_mode[1];

		set_float4(c, 0.8f, 0.8f, 0.8f, 1.0f);
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_DIFFUSE, &diffuse))
			color4_to_float4(&diffuse, c);
		//glMaterialfv(GL_FRONT_AND_BACK, GL_DIFFUSE, c);
		ms.diffuse.Set(c);

		set_float4(c, 0.0f, 0.0f, 0.0f, 1.0f);
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_SPECULAR, &specular))
			color4_to_float4(&specular, c);
		//glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, c);
		ms.specular.Set(c);

		set_float4(c, 0.2f, 0.2f, 0.2f, 1.0f);
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_AMBIENT, &ambient))
			color4_to_float4(&ambient, c);
		//glMaterialfv(GL_FRONT_AND_BACK, GL_AMBIENT, c);
		ms.ambient.Set(c);

		set_float4(c, 0.0f, 0.0f, 0.0f, 1.0f);
		if(AI_SUCCESS == aiGetMaterialColor(mtl, AI_MATKEY_COLOR_EMISSIVE, &emission))
			color4_to_float4(&emission, c);
		//glMaterialfv(GL_FRONT_AND_BACK, GL_EMISSION, c);
		ms.emission.Set(c);

		max = 1;
		ret1 = aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS, &shininess, &max);
		max = 1;
		ret2 = aiGetMaterialFloatArray(mtl, AI_MATKEY_SHININESS_STRENGTH, &strength, &max);

		if((ret1 == AI_SUCCESS) && (ret2 == AI_SUCCESS))
		{
			//glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, shininess * strength);

			ms.shininess=shininess*strength;
		}
		else 
		{
			//glMaterialf(GL_FRONT_AND_BACK, GL_SHININESS, 0.0f);
			ms.shininess=0;

			set_float4(c, 0.0f, 0.0f, 0.0f, 0.0f);
			//glMaterialfv(GL_FRONT_AND_BACK, GL_SPECULAR, c);
			
			ms.specular.Zero();
		}

		max = 1;
		if(aiGetMaterialIntegerArray(mtl, AI_MATKEY_ENABLE_WIREFRAME, &wireframe, &max)==AI_SUCCESS)
			ms.wireframe=wireframe;
		else
			ms.wireframe=false;

		max = 1;
		if(aiGetMaterialIntegerArray(mtl, AI_MATKEY_TWOSIDED, &two_sided, &max)==AI_SUCCESS)
			ms.two_sided=two_sided;
		else
			ms.two_sided=true;

		OutMaterial(ms);
		LOG_BR;
	}
}

template<typename T>
void AssimpLoader::SaveFaces(const aiFace *face,const T count,const OSString &extname)
{
	T *data=new T[count*3];

	const aiFace *sp=face;
	T *tp=data;

	for(T i=0;i<count;i++)
	{
		*tp=sp->mIndices[0];++tp;
		*tp=sp->mIndices[1];++tp;
		*tp=sp->mIndices[2];++tp;

		++sp;
	}

	SaveFile(data,sizeof(T)*3*count,extname);

	delete[] data;
}

void AssimpLoader::SaveTexCoord(const aiVector3D *texcoord,const uint comp,const uint count,const OSString &extname)
{
	if(comp<=0||comp>3)
		return;

	const uint size=1+sizeof(float)*comp*count;
	uchar *data=new uchar[size];

	const aiVector3D *sp=texcoord;
	float *tp=(float *)(data+1);

	*data=comp;

	if(comp==1)
	{
		for(uint i=0;i<count;i++)
		{
			*tp=sp->x;++tp;
			++sp;
		}
	}
	else
	if(comp==2)
	{
		for(uint i=0;i<count;i++)
		{
			*tp=sp->x;++tp;
			*tp=sp->y;++tp;
			++sp;
		}
	}
	else
	if(comp==3)
	{
		for(uint i=0;i<count;i++)
		{
			*tp=sp->x;++tp;
			*tp=sp->y;++tp;
			*tp=sp->z;++tp;
			++sp;
		}
	}

	SaveFile(data,size,extname);
	delete[] data;
}

#pragma pack(push,1)
struct MeshStruct
{
	uint32 primitive_type;				///<ͼԪ����

	uint32 vertices_number;				///<��������
	uint32 faces_number;				///<������

	uint32 color_channels;				///<����ɫ����
	uint32 texcoord_channels;			///<������������

	bool have_normal;					///<�Ƿ��з���
	bool have_tb;						///<�Ƿ������ߺ͸�����

	uint32 bones_number;				///<��������
};
#pragma pack(pop)

void AssimpLoader::LoadMesh()
{
	if(!scene->HasMeshes())return;

	UTF8String mesh_name;

	total_file_bytes=0;

	LOG_BR;
	LOG_INFO(OS_TEXT("Mesh Count ")+OSString(scene->mNumMeshes));

	uint vertex_total=0;
	uint tri_total=0;
	uint bone_total=0;

	for(unsigned int i=0;i<scene->mNumMeshes;i++)
	{
		const aiMesh *mesh=scene->mMeshes[i];

		if(mesh->mName.length)
			mesh_name=U8_TEXT("Mesh [")+UTF8String(i)+U8_TEXT(":")+UTF8String(mesh->mName.C_Str())+U8_TEXT("]");
		else
			mesh_name=U8_TEXT("Mesh [")+UTF8String(i)+U8_TEXT("]");
	
		int pn=mesh->mFaces[0].mNumIndices;

		LOG_BR;

		LOG_INFO(mesh_name+U8_TEXT(" PrimitiveTypes is ")+UTF8String(mesh->mPrimitiveTypes));

		LOG_INFO(mesh_name+U8_TEXT(" vertex color count is ")+UTF8String(mesh->GetNumColorChannels()));
		LOG_INFO(mesh_name+U8_TEXT(" texcoord count is ")+UTF8String(mesh->GetNumUVChannels()));

		if(mesh->GetNumUVChannels()>0)
			LOG_INFO(mesh_name+U8_TEXT(" Material Index is ")+UTF8String(mesh->mMaterialIndex));

		if(mesh->HasPositions())LOG_INFO(mesh_name+U8_TEXT(" have Position, vertices count ")+UTF8String(mesh->mNumVertices));
		if(mesh->HasFaces())LOG_INFO(mesh_name+U8_TEXT(" have Faces,faces count ")+UTF8String(mesh->mNumFaces));
		if(mesh->HasNormals())LOG_INFO(mesh_name+U8_TEXT(" have Normals"));
		if(mesh->HasTangentsAndBitangents())LOG_INFO(mesh_name+U8_TEXT(" have Tangent & Bitangents"));
		if(mesh->HasBones())LOG_INFO(mesh_name+U8_TEXT(" have Bones, Bones Count ")+UTF8String(mesh->mNumBones));

		vertex_total+=mesh->mNumVertices;
		tri_total+=mesh->mNumFaces;
		bone_total+=mesh->mNumBones;
		
		LOG_INFO(mesh_name+U8_TEXT(" PN is ")+UTF8String(pn));

		if(pn!=3)
			continue;

		{
			MeshStruct ms;

			ms.primitive_type	=HGL_PRIM_TRIANGLES;
			ms.vertices_number	=mesh->mNumVertices;
			ms.faces_number		=mesh->mNumFaces;
			ms.color_channels	=mesh->GetNumColorChannels();
			ms.texcoord_channels=mesh->GetNumUVChannels();
			ms.have_normal		=mesh->HasNormals();
			ms.have_tb			=mesh->HasTangentsAndBitangents();
			ms.bones_number		=mesh->mNumBones;

			SaveFile(&ms,sizeof(MeshStruct),OSString(i)+OS_TEXT(".mesh"));
		}

		const uint v3fdata_size=mesh->mNumVertices*3*sizeof(float);
		const uint v4fdata_size=mesh->mNumVertices*4*sizeof(float);

		if(mesh->HasPositions())
			SaveFile(mesh->mVertices,v3fdata_size,OSString(i)+OS_TEXT(".vertex"));

		if(mesh->HasNormals())
			SaveFile(mesh->mNormals,v3fdata_size,OSString(i)+OS_TEXT(".normal"));

		if(mesh->HasTangentsAndBitangents())
		{
			SaveFile(mesh->mTangents,v3fdata_size,OSString(i)+OS_TEXT(".tangent"));
			SaveFile(mesh->mBitangents,v3fdata_size,OSString(i)+OS_TEXT(".bitangent"));
		}

		for(int c=0;c<mesh->GetNumColorChannels();c++)
			SaveFile(mesh->mColors[c],v4fdata_size,OSString(i)+OS_TEXT(".")+OSString(c)+OS_TEXT(".color"));

		for(int c=0;c<mesh->GetNumUVChannels();c++)
			SaveTexCoord(mesh->mTextureCoords[c],mesh->mNumUVComponents[c],mesh->mNumVertices,OSString(i)+OS_TEXT(".")+OSString(c)+OS_TEXT(".texcoord"));

		if(mesh->HasFaces())
		{
			if(mesh->mNumVertices>0xFFFF)
				SaveFaces<uint32>(mesh->mFaces,mesh->mNumFaces,OSString(i)+OS_TEXT(".face"));
			else
				SaveFaces<uint16>(mesh->mFaces,mesh->mNumFaces,OSString(i)+OS_TEXT(".face"));
		}
	}

	LOG_BR;
	LOG_INFO(OS_TEXT("TOTAL Vertices Count ")+OSString(vertex_total));
	LOG_INFO(OS_TEXT("TOTAL Faces Count ")+OSString(tri_total));
	LOG_INFO(OS_TEXT("TOTAL Bones Count ")+OSString(bone_total));

	LOG_INFO(OS_TEXT("TOTAL File Size ")+OSString(total_file_bytes));
	LOG_BR;
}

void AssimpLoader::LoadScene(const OSString &front,SceneNode *node,const aiScene *sc,const aiNode *nd)
{
	aiMatrix4x4 m=nd->mTransformation;

	aiTransposeMatrix4(&m);

	node->SetLocalMatrix(matrix_convert(m));		//ǿ��COPY������������

	for(unsigned int n=0;n<nd->mNumMeshes;++n)
	{
//		node->SubData.Add(mesh_list[nd->mMeshes[n]]);

		const aiMesh *mesh=scene->mMeshes[nd->mMeshes[n]];

		LOG_INFO(front+OS_TEXT("Mesh[")+OSString(nd->mMeshes[n])+OS_TEXT("] MaterialID[")+OSString(mesh->mMaterialIndex)+OS_TEXT("]"));
	}

	const OSString new_front=OS_TEXT("   +")+front;

	for(unsigned int n=0;n<nd->mNumChildren;++n)
	{
		SceneNode* sub_node=new SceneNode;

		LoadScene(new_front,sub_node,sc,nd->mChildren[n]);

		node->AddSubNode(sub_node);
	}
}

bool AssimpLoader::LoadFile(const OSString &filename)
{
	uint filesize;
	char *filedata;
	
	filesize=filesystem::LoadFileToMemory(filename,(void **)&filedata);

	if(!filedata)
		return(false);

	scene=aiImportFileFromMemory(filedata,filesize,aiProcessPreset_TargetRealtime_MaxQuality,nullptr);
	
	delete[] filedata;

	if(!scene)
		return(false);

	filename.SubString(main_filename,0,filename.FindRightChar(OS_TEXT('.')));

	get_bounding_box(&scene_min,&scene_max);

	scene_center.x = (scene_min.x + scene_max.x) / 2.0f;
	scene_center.y = (scene_min.y + scene_max.y) / 2.0f;
	scene_center.z = (scene_min.z + scene_max.z) / 2.0f;

	//���˳���Ǽ��������еĲ��ʺ�ģ�Ͷ��б�ʹ��
	//aiProcessPreset_TargetRealtime_Quality�Ѿ�ָ���˻�ɾ��������ʣ��������ﲻ�ô�����
	//������ģ�Ͳ���ȷ���Ƿ����

	SceneNode root;

	LoadMaterial();								//�������в���
	LoadMesh();									//��������mesh
	LoadScene(OSString(OS_TEXT("")),&root,scene,scene->mRootNode);	//���볡���ڵ�

	//root.RefreshMatrix();						//ˢ�¾���

	//root.ExpendToList(&render_list);			//չ������Ⱦ�б�

//	root.SaveToStream(&FileStream(L"Root.scene"));
	
	return(true);
}
