#ifndef MESH_TO_MATRIX
#define MESH_TO_MATRIX

#include <vector>
#include <list>
#include <utility>
#include <Eigen/Dense>
#include <vcg/complex/algorithms/update/topology.h>
#include <vcg/complex/complex.h>

using namespace std;

namespace vcg{
template < typename TriMeshType >
class MeshToMatrix
{

    // define types

    typedef typename TriMeshType::FaceType FaceType;
    typedef typename TriMeshType::VertexType VertexType;
    typedef typename TriMeshType::CoordType CoordType;
    typedef typename TriMeshType::ScalarType ScalarType;

    static void GetTriEdgeAdjacency(const Eigen::MatrixXd& V,
                                    const Eigen::MatrixXi& F,
                                    Eigen::MatrixXi& EV,
                                    Eigen::MatrixXi& FE,
                                    Eigen::MatrixXi& EF)
    {
        //assert(igl::is_manifold(V,F));
        std::vector<std::vector<int> > ETT;
        for(int f=0;f<F.rows();++f)
            for (int i=0;i<3;++i)
            {
                // v1 v2 f vi
                int v1 = F(f,i);
                int v2 = F(f,(i+1)%3);
                if (v1 > v2) std::swap(v1,v2);
                std::vector<int> r(4);
                r[0] = v1; r[1] = v2;
                r[2] = f;  r[3] = i;
                ETT.push_back(r);
            }
        std::sort(ETT.begin(),ETT.end());

        // count the number of edges (assume manifoldness)
        int En = 1; // the last is always counted
        for(unsigned i=0;i<ETT.size()-1;++i)
            if (!((ETT[i][0] == ETT[i+1][0]) && (ETT[i][1] == ETT[i+1][1])))
                ++En;

        EV = Eigen::MatrixXi::Constant((int)(En),2,-1);
        FE = Eigen::MatrixXi::Constant((int)(F.rows()),3,-1);
        EF = Eigen::MatrixXi::Constant((int)(En),2,-1);
        En = 0;

        for(unsigned i=0;i<ETT.size();++i)
        {
            if (i == ETT.size()-1 ||
                    !((ETT[i][0] == ETT[i+1][0]) && (ETT[i][1] == ETT[i+1][1]))
                    )
            {
                // Border edge
                std::vector<int>& r1 = ETT[i];
                EV(En,0)     = r1[0];
                EV(En,1)     = r1[1];
                EF(En,0)    = r1[2];
                FE(r1[2],r1[3]) = En;
            }
            else
            {
                std::vector<int>& r1 = ETT[i];
                std::vector<int>& r2 = ETT[i+1];
                EV(En,0)     = r1[0];
                EV(En,1)     = r1[1];
                EF(En,0)    = r1[2];
                EF(En,1)    = r2[2];
                FE(r1[2],r1[3]) = En;
                FE(r2[2],r2[3]) = En;
                ++i; // skip the next one
            }
            ++En;
        }

        // Sort the relation EF, accordingly to EV
        // the first one is the face on the left of the edge

        for(unsigned i=0; i<EF.rows(); ++i)
        {
            int fid = EF(i,0);
            bool flip = true;
            // search for edge EV.row(i)
            for (unsigned j=0; j<3; ++j)
            {
                if ((F(fid,j) == EV(i,0)) && (F(fid,(j+1)%3) == EV(i,1)))
                    flip = false;
            }

            if (flip)
            {
                int tmp = EF(i,0);
                EF(i,0) = EF(i,1);
                EF(i,1) = tmp;
            }
        }
    }

public:

    // return mesh as vactor of vertices and faces
    static void GetTriMeshData(const TriMeshType &mesh,
                               Eigen::MatrixXi &faces,
                               Eigen::MatrixXd &vert)
    {
        // create eigen matrix of vertices
        vert=Eigen::MatrixXd(mesh.VN(), 3);

        // copy vertices
        for (int i = 0; i < mesh.VN(); i++)
            for (int j = 0; j < 3; j++)
                vert(i,j) = mesh.vert[i].cP()[j];

        // create eigen matrix of faces
        faces=Eigen::MatrixXi(mesh.FN(), 3);

        // copy faces
        const VertexType *v0 = &mesh.vert[0];
        for (int i = 0; i < mesh.FN(); i++)
            for (int j = 0; j < 3; j++)
            {
                faces(i,j) = (int)(mesh.face[i].V(j) - v0);
                assert(faces(i,j) >= 0 && faces(i,j) < mesh.VN());
            }
    }

    // return normals of the mesh
    static void GetNormalData(const TriMeshType &mesh,
                              Eigen::MatrixXd &Nvert,
                              Eigen::MatrixXd &Nface)
    {
        // create eigen matrix of vertices
        Nvert=Eigen::MatrixXd(mesh.VN(), 3);
        Nface=Eigen::MatrixXd(mesh.FN(), 3);

        // per vertices normals
        for (int i = 0; i < mesh.VN(); i++)
            for (int j = 0; j < 3; j++)
                Nvert(i,j) = mesh.vert[i].cN()[j];

        // per vertices normals
        for (int i = 0; i < mesh.FN(); i++)
            for (int j = 0; j < 3; j++)
                Nface(i,j) = mesh.face[i].cN()[j];
    }

    // get face to face adjacency
    static void GetTriFFAdjacency(TriMeshType &mesh,
                                  Eigen::MatrixXi &FFp,
                                  Eigen::MatrixXi &FFi)
    {
        vcg::tri::UpdateTopology<TriMeshType>::FaceFace(mesh);
//        FFp = Eigen::PlainObjectBase<int>::Constant(mesh.FN(),3,-1);
//        FFi = Eigen::PlainObjectBase<int>::Constant(mesh.FN(),3,-1);

        FFp = Eigen::MatrixXi(mesh.FN(),3);
        FFi = Eigen::MatrixXi(mesh.FN(),3);

        for (int i = 0; i < mesh.FN(); i++)
            for (int j = 0; j < 3; j++)
            {
                FaceType *AdjF=mesh.face[i].FFp(j);
                if (AdjF==&mesh.face[i])
                {
                    FFp(i,j)=-1;
                    FFi(i,j)=-1;
                    continue;
                }

                int AdjF_Index=vcg::tri::Index(mesh,AdjF);
                int OppF_Index=mesh.face[i].FFi(j);


                FFp(i,j)=AdjF_Index;
                FFi(i,j)=OppF_Index;

                assert(AdjF_Index >= 0 && AdjF_Index < mesh.FN());
            }
    }

    // get edge to face and edge to vertex adjacency
    static void GetTriEdgeAdjacency(const TriMeshType &mesh,
                                    Eigen::MatrixXi& EV,
                                    Eigen::MatrixXi& FE,
                                    Eigen::MatrixXi& EF)
    {
        Eigen::MatrixXi faces;
        Eigen::MatrixXd vert;
        GetTriMeshData(mesh,faces,vert);
        GetTriEdgeAdjacency(vert,faces,EV,FE,EF);
    }

    static Eigen::Vector3d VectorFromCoord(const CoordType &v)
    {
        // create eigen vector
        Eigen::Vector3d c;

        // copy coordinates
        for (int i = 0; i < 3; i++)
            c(i) = v[i];

        return c;
    }

};
}

#endif // MESH_TO_MATRIX_CONVERTER
