from pathlib import Path
from launch import LaunchDescription
from launch.actions import DeclareLaunchArgument
from launch.conditions import IfCondition
from launch.substitutions import LaunchConfiguration
from launch_ros.actions import Node
from launch_ros.parameter_descriptions import ParameterValue


def generate_launch_description():
    rviz_config = Path(__file__).resolve().parent.parent / 'config' / 'wlr716.rviz'
    return LaunchDescription([
        DeclareLaunchArgument('hostname', default_value='192.168.100.104'),
        DeclareLaunchArgument('port', default_value='2110'),
        DeclareLaunchArgument('rviz', default_value='true'),
        Node(package='wj_716_lidar', executable='wj_716_lidar_node', output='screen',
             parameters=[{'hostname': LaunchConfiguration('hostname'),
                          'port': ParameterValue(LaunchConfiguration('port'), value_type=int),
                          'frame_id': 'vanjee_lidar', 'scan_topic': '/scan'}]),
        Node(package='rviz2', executable='rviz2', output='screen',
             arguments=['-d', str(rviz_config)], condition=IfCondition(LaunchConfiguration('rviz'))),
    ])
