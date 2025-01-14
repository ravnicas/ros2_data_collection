"""Launch save image node."""

import launch
import launch_ros.actions


def generate_launch_description() -> launch.LaunchDescription:
    """Launch file generator.

    Returns:
        launch.LaunchDescription: Launch
    """
    barcode_node = launch_ros.actions.Node(
        package="dc_services",
        executable="save_image",
        namespace="/dc/service",
        output={
            "stdout": "screen",
            "stderr": "screen",
        },
    )

    nodes = [barcode_node]
    return launch.LaunchDescription(nodes)
