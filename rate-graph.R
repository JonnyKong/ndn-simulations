# Copyright (c) 2012,2015  Alexander Afanasyev <alexander.afanasyev@ucla.edu>

# install.packages('ggplot2')
library(ggplot2)
# install.packages('scales')
library(scales)

# install.packages('doBy')
library(doBy)

#########################
# Rate trace processing #
#########################
data = read.table("result/test-rate-trace.txt", header=T)
data$Node = factor(data$Node)
data$FaceId <- factor(data$FaceId)
data$Kilobits <- data$Kilobytes * 8
data$Type = factor(data$Type)

# exlude irrelevant types
data = subset(data, Type %in% c("OutData"))
data = subset(data, Time >= 5)
#data = subset(data, Type %in% c("InData", "OutData"))
#data = subset(data, Type %in% c("InInterests", "OutInterests", "InData", "OutData"))

# combine stats from all faces
#data.combined = summaryBy(. ~ Time + Node + Type, data=data, FUN=sum)
data.combined = summaryBy(. ~ Time + Type, data=data, FUN=sum)

#data.dest = subset (data.combined, Node == "99")
#data.leaves = subset(data.combined, Node %in% c("leaf-1", "leaf-2", "leaf-3", "leaf-4"))

print(data.combined)
'''
devtools::install_github("guiastrennec/ggplus")
library(ggplus)

# graph rates on all nodes in Kilobits
g.all <- ggplot(data.combined, aes(x=Time, y=Kilobits.sum, color=Type)) +
  geom_point(size=0.5) +
  geom_line() +
  ylab("Rate [Kbits/s]") +
  #facet_wrap(~ Node) +
  theme_bw()

#g.all <- ggplot(data.combined) +
#  geom_point(aes (x=Time, y=Kilobits.sum, color=Type), size=2) +
#  geom_line(aes (x=Time, y=Kilobits.sum, color=Type), size=2) +
#  scale_size_continuous(range = c(0,2.5)) +
#  ylab("Rate [Kbits/s]") 
#  facet_wrap(~ Node)
#g.all <- facet_multiple(plot=g.all, facets="Node", ncol = 5, nrow = 5)
print(g.all)
1
# graph rates on the root nodes in Packets
#g.dest <- ggplot(data.dest) +
#  geom_point(aes (x=Time, y=Kilobits.sum, color=Type), size=1) +
#  geom_line(aes (x=Time, y=Kilobits.sum, color=Type), size=0.5) +
#  ylab("Rate [Kbits/s]")

#print(g.dest)

#png("root-rates-geo.png", width=500, height=250)
#print(g.node5)
retval <- dev.off()
'''