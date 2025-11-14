from pygccxml import declarations


class HeuristicProcessor:
    """
    分析一个类并根据启发式规则生成特殊的C++序列化代码。
    """

    def __init__(self, cls_decl: declarations.class_t):
        self.cls = cls_decl
        self.cls_name = cls_decl.name
        self.handler_code = []
        self.required_includes = set()

    def generate_handler(self):
        """
        主入口函数，应用所有启发式规则。
        返回 (handler_code_string, required_includes_set) 或 (None, None)。
        """

        has_location = self.cls.member_function("Location")
        has_orientation = self.cls.member_function("Orientation")

        if has_location and has_orientation:
            self._apply_topods_shape_heuristics()

        if not self.handler_code:
            return None, None

        return "\\n".join(self.handler_code), self.required_includes

    def _apply_topods_shape_heuristics(self):
        """应用所有与TopoDS_Shape相关的启发式规则。"""
        self.required_includes.update([
            "<TopoDS.hxx>",
            "<BRep_Tool.hxx>",
            "<TopLoc_Location.hxx>",
            "\"OCCTValueConverter.h\""
        ])

        # 注入通用 Shape 处理代码
        self.handler_code.append(
            """
    const auto& shape = std::any_cast<const TopoDS_Shape&>(obj_any);
    TopLoc_Location location = shape.Location();
    if (!location.IsIdentity()) {
        serializer.serialize_transient_and_link(location.FirstDatum(), node.temp_id, "HAS_LOCATION");
    }
    node.properties["orientation"] = static_cast<int32_t>(shape.Orientation());
"""
        )

        # 启发式规则 1.1: Vertex -> BRep_Tool::Pnt
        # 触发条件：类名是 TopoDS_Vertex
        if self.cls_name == "TopoDS_Vertex":
            self.handler_code.append(
                """
    gp_Pnt pnt = BRep_Tool::Pnt(TopoDS::Vertex(shape));
    node.properties["geometry"] = OCCTValueConverter::to_serializable(std::any(pnt));
"""
            )

        # 启发式规则 1.2: Edge -> BRep_Tool::Curve
        # 触发条件：类名是 TopoDS_Edge
        elif self.cls_name == "TopoDS_Edge":
            self.handler_code.append(
                """
    Standard_Real first, last;
    Handle(Geom_Curve) curve = BRep_Tool::Curve(TopoDS::Edge(shape), first, last);
    serializer.serialize_transient_and_link(curve, node.temp_id, "GEOMETRY");
    node.properties["range_first"] = first;
    node.properties["range_last"] = last;
"""
            )

        # 启发式规则 1.3: Face -> BRep_Tool::Surface
        # 触发条件：类名是 TopoDS_Face
        elif self.cls_name == "TopoDS_Face":
            self.handler_code.append(
                """
    Handle(Geom_Surface) surface = BRep_Tool::Surface(TopoDS::Face(shape));
    serializer.serialize_transient_and_link(surface, node.temp_id, "GEOMETRY");
"""
            )
