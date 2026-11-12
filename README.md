# rosbag2_snapshot

针对[此 rosbag2 issue](https://github.com/ros2/rosbag2/issues/663) 的解决方案，功能类似于 [`rosbag_snapshot`](https://github.com/ros/rosbag_snapshot)。这里将其作为独立包添加，而非直接修改 `rosbag2`，因为 ROS 1 中也是如此实现的 ;)。

它订阅话题并像行车记录仪一样维护一个近期消息的缓冲区。在实车测试中非常有用——如果意外的场景发生了，但此前没有运行 `rosbag record`（磁盘空间限制使得持续运行 `rosbag record` 不现实），就会错失数据。相反，用户可以在后台运行 snapshot，并按需将最近一段时间的数据保存到磁盘。


## 使用方法

`rosbag2_snapshot` 可以通过 ROS 参数进行更细粒度的控制。`snapshotter` 命令将运行缓冲区服务端，`snapshotter_client` 命令可作为客户端使用，请求服务端将数据写入磁盘或冻结缓冲区以保留感兴趣的数据，等待用户决定保存哪些内容。

### 服务端

```
$ ros2 run rosbag2_snapshot snapshotter
```

缓冲近期消息，直到被触发写入，或触发一个已在运行的实例。

### 示例参数文件
```yaml
/**:
  ros__parameters:
    default_duration_limit: 10.0           # [可选, 默认=30.0] 缓冲区中最新与最旧消息的最大时间差（秒）
    default_memory_limit: 64.0             # [可选, 默认=-1] 每个话题缓冲区中消息使用的最大内存（MB）
    storage: "sqlite3"                  # [可选, 默认="sqlite3"] 存储格式后端。使用 "mcap" 以支持 MCAP 格式。
    clear_buffer: true                      # [可选, 默认=true] 是否在恢复时清空缓冲区。设为 false 以保留缓冲数据。
    storage_preset_profile: ""             # [可选, 默认=""] 选择存储配置预设（例如 "resilient"）
    storage_config_file: ""                # [可选, 默认=""] 存储特定配置的 YAML 文件路径
    topics: ["/topic1", "/topic2"]         # [可选] 要缓冲的话题列表。为空则缓冲所有话题。
    topic_details:
      /topic1:
        type: "sensor_msgs/msg/NavSatFix"  # [若指定话题则必填] 话题类型
      /topic2:
        type: "sensor_msgs/msg/Odometry"
        duration: 15.0                     # [可选] 覆盖持续时长限制，继承内存限制
      /topic3:
        type: "sensor_msgs/msg/Image"
        duration: 2.0                      # [可选] 同时覆盖两个限制
        memory: -1                         # 负值表示无限制
```

###### 带命令行参数的启动示例

``` bash
ros2 run rosbag2_snapshot snapshotter --ros-args -p default_duration_limit:=10.0 -p storage:='mcap'
```

###### 如需在暂停恢复时保留缓冲数据，将 `clear_buffer` 设为 `false`：

```bash
ros2 run rosbag2_snapshot snapshotter --ros-args -p clear_buffer:=false
```

###### 使用存储配置预设（例如 "resilient"）
```bash
ros2 run rosbag2_snapshot snapshotter --ros-args -p storage_preset_profile:="resilient"
```

###### 使用存储配置文件
```bash
ros2 run rosbag2_snapshot snapshotter --ros-args -p storage_config_file:="/path/to/storage_config.yaml"
```

### 客户端

###### 将所有缓冲数据写入 `<datetime>.bag`
```bash
ros2 run rosbag2_snapshot snapshotter_client --ros-args -p action_type:=trigger_write
```

###### 将指定话题的缓冲数据写入 `new_lighting<datetime>.bag`
```bash
ros2 run rosbag2_snapshot snapshotter_client --ros-args -p action_type:=trigger_write -p filename:=new_lighting -p topics:=["/camera/image_raw", "/camera/camera_info"]
```

###### 将所有缓冲数据写入 `/home/user/crashed_into_wall.bag`
```bash
ros2 run rosbag2_snapshot snapshotter_client --ros-args -p action_type:=trigger_write -p filename:="/home/user/crashed_into_wall.bag"
```

###### 暂停新数据缓冲，将当前缓冲区保留在内存中，直到恢复或触发写入
```bash
ros2 run rosbag2_snapshot snapshotter_client --ros-args -p action_type:=pause
```

###### 恢复新数据缓冲
```bash
ros2 run rosbag2_snapshot snapshotter_client --ros-args -p action_type:=resume
```

###### 手动调用 trigger 服务

```
$ ros2 service call /trigger_snapshot rosbag2_snapshot_msgs/srv/TriggerSnapshot "{filename: '', topics: [], start_time: {sec: 0, nanosec: 0}, stop_time: {sec: 0, nanosec: 0}}"
requester: making request: rosbag2_snapshot_msgs.srv.TriggerSnapshot_Request(filename='', topics=[], start_time=builtin_interfaces.msg.Time(sec=0, nanosec=0), stop_time=builtin_interfaces.msg.Time(sec=0, nanosec=0))

response:
rosbag2_snapshot_msgs.srv.TriggerSnapshot_Response(success=True, message='')
```

###### 手动调用 pause/resume 服务

```
$ ros2 service call /enable_snapshot std_srvs/srv/SetBool "{data: false}"
requester: making request: std_srvs.srv.SetBool_Request(data=False)

response:
std_srvs.srv.SetBool_Response(success=True, message='')
```