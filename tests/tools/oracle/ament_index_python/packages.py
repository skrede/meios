import os


class PackageNotFoundError(KeyError):
    pass


def get_package_share_directory(name):
    for root in os.environ.get("ORACLE_PACKAGE_ROOTS", "").split(os.pathsep):
        if root and os.path.isdir(os.path.join(root, name)):
            return os.path.join(root, name)
    raise PackageNotFoundError(name)
