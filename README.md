#题目：高效用户态线程设计与调度

##赛题介绍

用户态线程是实现parallel loop和SIMD的一个典型途径，例如openmp，已成为系统的基础组件。他自动将数据分成多块，并分发给多个线程并行处理。但现有用户态线程系统由于负载均衡、调度分发等问题，不能高效地支持嵌套并行、应用混布等场景。该题目设计新的用户态线程架构和调度算法，并移植到更多场景应用。

##评分标准

- **功能完整性：**设计并实现用户态线程系统及其调度算法。
- **可扩展性：**讲用户态线程移植到更多场景应用中。
- **优化性：**能够根据应用要求，优化多个纬度的指标，例如实时性、资源占用、能耗等。
- **创新性：**
  - 新的用户态线程架构，突破现有基于内核进的设计，实现更轻量、扩展性更好的架构设计。
  - 新的调度算法，针对实时要求，负载均衡，资源利用率等要求，设计更优的调度算法。


##交付件

- **代码：**内存管理的源代码，具有良好的注释和文档；
- **文档：**系统的设计文档，包括系统架构、策略算法、数据结构等；
- **测试：**系统的测试报告，包括测试环境、测试用例、测试结果等。

##参考资料

论文：Maximizing systemutilizationviaparallelismmanagementforco-locatedparallelapplications（PACT'18)
论文：Callisto: co-scheduling parallel runtime systems（EuroSys'14)
论文：Parcae: a system forflexibleparallelexecution（PLDI'12)
论文：FJOS: Practical, Predictable, and Efficient System Support for Fork/Join Parallelism (RTAS'14)

## 项目导师

- 任玉鑫 email:renyuxin1@huawei.com
