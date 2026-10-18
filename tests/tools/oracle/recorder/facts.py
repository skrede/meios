def children(node, tag):
    return [c for c in node.childNodes if c.nodeType == c.ELEMENT_NODE and c.tagName == tag]


def attribute_text(node, names):
    return " ".join("{}={}".format(n, node.getAttribute(n)) for n in names if node.hasAttribute(n))


def origin_rows(link):
    rows = []
    for kind in ("visual", "collision"):
        for at, part in enumerate(children(link, kind)):
            for origin in children(part, "origin"):
                key = "origin.{}.{}".format(link.getAttribute("name"), kind)
                rows.append((key if not at else "{}.{}".format(key, at),
                             attribute_text(origin, ("xyz", "rpy"))))
    return rows


# force_abs_paths renders a mesh URI as file://$(find <package>)/..., which bakes this recording
# run's own absolute build-tree location. Normalized to a portable file://<package-root>/... form
# so the record stays reproducible across build trees; the C++ comparison applies the identical
# normalization to its own resolved package root before comparing, so both sides agree regardless
# of where either one's package root actually sits on disk.
def portable_mesh_uri(filename, package_root):
    prefix = "file://" + str(package_root)
    if filename.startswith(prefix):
        return "file://<package-root>" + filename[len(prefix):]
    return filename


# Only the attributes the element declares are recorded, so a guard naming two of the four is
# recorded as naming two rather than as naming four of which two are zero.
def joint_rows(joints):
    rows = []
    for joint in joints:
        name = joint.getAttribute("name")
        rows.append(("joint.type." + name, joint.getAttribute("type")))
        for limit in children(joint, "limit"):
            rows.append(("joint.limit." + name,
                         attribute_text(limit, ("lower", "upper", "effort", "velocity"))))
        for guard in children(joint, "safety_controller"):
            rows.append(("joint.safety." + name,
                         attribute_text(guard, ("soft_lower_limit", "soft_upper_limit",
                                                "k_position", "k_velocity"))))
    return rows


# The model is the link and joint children of the robot root and nothing else. A sweep of the whole
# document also collects the <joint> elements an extension block declares -- a ros2_control interface
# list names one per actuated joint, with no type and no limit -- which would record a joint twice
# under contradicting values and count a model this project never builds.
def facts_rows(doc, package_root):
    links = children(doc.documentElement, "link")
    joints = children(doc.documentElement, "joint")
    rows = [("link.count", str(len(links))), ("joint.count", str(len(joints)))]
    rows += [("link.name.{}".format(at), l.getAttribute("name")) for at, l in enumerate(links)]
    rows += [("joint.name.{}".format(at), j.getAttribute("name")) for at, j in enumerate(joints)]
    rows += joint_rows(joints)
    meshes = []
    for link in links:
        for mesh in link.getElementsByTagName("mesh"):
            filename = portable_mesh_uri(mesh.getAttribute("filename"), package_root)
            if filename not in meshes:
                meshes.append(filename)
    rows += [("mesh.filename.{}".format(at), m) for at, m in enumerate(meshes)]
    for link in links:
        rows += origin_rows(link)
    return rows


def robot_facts(root, document, mappings):
    import xacro

    return facts_rows(xacro.process_file(str(root / document), mappings=mappings), root.parent)

