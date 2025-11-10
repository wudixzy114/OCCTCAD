#include <iostream>

// 引入 OCCT 的头文件
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

int main()
{
    // 创建一个三维点
    gp_Pnt aP1(1.0, 2.0, 3.0);

    // 创建一个三维向量
    gp_Vec aVec(4.0, 5.0, 6.0);

    // 点通过向量进行移动，得到新点
    gp_Pnt aP2 = aP1.Translated(aVec);

    // 打印新点的坐标
    std::cout << "OCCT Test Succeeded!" << std::endl;
    std::cout << "New Point Coordinates: ("
        << aP2.X() << ", "
        << aP2.Y() << ", "
        << aP2.Z() << ")" << std::endl;

    return 0;
}